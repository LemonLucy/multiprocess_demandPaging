#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define PAGESIZE (32)
#define PAS_FRAMES (256)
#define PAS_SIZE (PAGESIZE * PAS_FRAMES)
#define VAS_PAGES (64)
#define VAS_SIZE (PAGESIZE * VAS_PAGES)
#define PTE_SIZE (4)
#define PAGETABLE_FRAMES ((VAS_PAGES * PTE_SIZE) / PAGESIZE)
#define PAGE_INVALID (0)
#define PAGE_VALID (1)
#define MAX_REFERENCES (256)
#define MAX_PROCESSES (100) // 최대 프로세스 수를 정의

typedef struct {
    unsigned char frame;
    unsigned char vflag;
    unsigned char ref;
} pte;

typedef struct {
    int pid;
    int ref_len;
    unsigned char *reference;
    pte page_table[VAS_PAGES]; // 각 프로세스마다 개별 페이지 테이블
    int cnt;
} process;

typedef struct {
    unsigned char b[PAGESIZE];
} frame;

void load_process(process *proc, int pid, unsigned char *references, int ref_len) {
    proc->pid = pid;
    proc->ref_len = ref_len;
    proc->reference = references;
    for (int i = 0; i < VAS_PAGES; i++) {
        proc->page_table[i].frame = 0;
        proc->page_table[i].vflag = PAGE_INVALID;
        proc->page_table[i].ref = 0;
    }
    proc->cnt=0;
}

void print_page_table(process *procs, int num_procs, int *allocated_frame, int *PF) {
    for (int i = 0; i < num_procs; i++) {
        printf("** Process %03d: Allocated Frames=%03d PageFaults/References=%03d/%03d\n", i, allocated_frame[i], PF[i], procs[i].cnt);
        pte *PT = procs[i].page_table; // 프로세스의 페이지 테이블 가져오기
        for (int j = 0; j < VAS_PAGES; j++) {
            if (PT[j].vflag == PAGE_VALID) {
                printf("%03d -> %03d REF=%03d\n", j, PT[j].frame, PT[j].ref);
            }
        }
    }
    int total_frame = 0;
    int total_pf = 0;
    int total_cnt = 0;
    for (int i = 0; i < num_procs; i++) {
        total_cnt += procs[i].cnt;
        total_frame += allocated_frame[i];
        total_pf += PF[i];
    }
    printf("Total: Allocated Frames=%03d Page Faults/References=%03d/%03d\n", total_frame, total_pf, total_cnt);
}
void page_fault_handler(pte *PT, int page_number, frame *pas, int *frame_num, int *allocated_frame, int *PF) {
    PT[page_number].ref = 1;//해당 page number 참조횟수
    PT[page_number].frame = *frame_num;
    PT[page_number].vflag = PAGE_VALID;
    pas[*frame_num].b[0] = page_number; // 페이지 번호를 프레임에 매핑
    //printf("PF, Allocated Frame %03d \n", PT[page_number].frame);
    (*frame_num)++;//그다음 frame번호로 넘어감
    (*allocated_frame)++;//할당된 프레임 갯수 +1
    (*PF)++;//page fault횟수 +1
}

void execute_processes(process *procs, int num_procs, frame *pas, int frame_num) {
    int idx[num_procs];
    // 참조 인덱스와 총 참조 수를 초기화
    for (int i = 0; i < num_procs; i++) {
        idx[i] = 0;
    }
    int *PF = (int *)malloc(num_procs * sizeof(int));
    int *allocated_frame = (int *)malloc(num_procs * sizeof(int));

    // 배열을 0으로 초기화
    for (int i = 0; i < num_procs; i++) {
        PF[i] = 0;
        allocated_frame[i] = 8;
    }

    bool completed = false;
    while (!completed) {
        completed = true;
        for (int i = 0; i < num_procs; i++) {
            if (idx[i] < procs[i].ref_len) { // 아직 참조할 페이지가 남았는지 확인
                completed = false;
                int page_number = procs[i].reference[idx[i]]; // 참조할 페이지 번호 가져옴
                //printf("[PID %02d REF:%03d] Page access %03d : ", procs[i].pid, idx[i], page_number);

                pte *PT = procs[i].page_table; // 프로세스의 페이지 테이블 가져오기
                // 페이지 테이블을 사용하여 페이지를 메모리에 매핑
                if (PT[page_number].vflag == PAGE_VALID) {
                    PT[page_number].ref += 1;
                    //printf("Frame %03d\n", PT[page_number].frame);
                } else {
                    if (frame_num >= PAS_FRAMES) {//메모리 부족 종료
                        printf("Out of memory!!\n");
                        print_page_table(procs, num_procs, allocated_frame, PF);
                        free(PF);
                        free(allocated_frame);
                        return;
                    }
                    // page fault 처리
                    page_fault_handler(PT, page_number, pas, &frame_num, &allocated_frame[i], &PF[i]);
                }
                idx[i]++;
                procs[i].cnt++;
            }
        }
    }

    // 실행 후 결과 출력
    print_page_table(procs, num_procs, allocated_frame, PF);

    // 메모리 해제
    free(PF);
    free(allocated_frame);
}

//파일에서 각각의 process 읽어들임
int read_processes(FILE *fp, process *procs) {
    int num_procs = 0;
    while (!feof(fp)) {
        int pid, ref_len;
        if (fread(&pid, sizeof(int), 1, fp) != 1) break;
        if (fread(&ref_len, sizeof(int), 1, fp) != 1) break;
        unsigned char *references = (unsigned char *)malloc(ref_len * sizeof(unsigned char));
        if (fread(references, sizeof(unsigned char), ref_len, fp) != ref_len) break;
        load_process(&procs[num_procs], pid, references, ref_len);
        num_procs++;
    }
    return num_procs;
}
int main(int argc, char *argv[]) {
    FILE *fp;
    if (argc == 1) {
        fp = stdin;
    } else if (argc == 2) {
        fp = fopen(argv[1], "rb");
        if (fp == NULL) {
            fprintf(stderr, "Failed to open file: %s\n", argv[1]);
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    process procs[MAX_PROCESSES];
    frame *pas = (frame *)malloc(PAS_SIZE); //물리메모리 frame 배열
    int num_procs = read_processes(fp, procs);
    fclose(fp);

    int initial_frame_num = num_procs * 8; //하나의 process마다 8개의 frame씩 초기에 할당되고 나서의 frame번호
    execute_processes(procs, num_procs, pas, initial_frame_num);
    //메모리해제
    for (int i = 0; i < num_procs; i++) {
        free(procs[i].reference);
    }
    free(pas);

    return 0;
}