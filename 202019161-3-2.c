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
    proc->cnt = 0;
}

void print_page_table(process *procs, int num_procs, int *allocated_frame, int *PF, int **lv1) {
    for (int i = 0; i < num_procs; i++) {
        printf("** Process %03d: Allocated Frames=%03d PageFaults/References=%03d/%03d\n", i, allocated_frame[i], PF[i], procs[i].cnt);
        for (int j = 0; j < 8; j++) {
            if (lv1[i][j] != -1) {
                printf("(L1PT) %03d -> %03d\n", j, lv1[i][j]);
                for (int k = 0; k < VAS_PAGES / 8; k++) {
                    int lv2_idx = j * 8 + k;
                    if (procs[i].page_table[lv2_idx].vflag == PAGE_VALID) {
                        printf("(L2PT) %03d -> %03d REF=%03d\n", lv2_idx, procs[i].page_table[lv2_idx].frame, procs[i].page_table[lv2_idx].ref);
                    }
                }
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
void page_fault(process *proc, int page_number, int *frame_num, int *allocated_frame, int *PF) {
    (*frame_num) += 1;
    (*allocated_frame) += 1;
    (*PF) += 1;
    proc->page_table[page_number].vflag = PAGE_VALID;
    proc->page_table[page_number].frame = *frame_num;
    proc->page_table[page_number].ref = 1;
}

bool check_memory(int frame_num, process *procs, int num_procs, int *allocated_frame, int *PF, int **lv1) {
    if (frame_num >= PAS_FRAMES-1) {
        printf("Out of memory!!\n");
        print_page_table(procs, num_procs, allocated_frame, PF, lv1);
        free(PF);
        free(allocated_frame);
        for (int i = 0; i < num_procs; i++) {
            free(lv1[i]);
        }
        free(lv1);
        return false;
    }
    return true;
}

void execute_processes(process *procs, int num_procs, int frame_num) {
    int idx[num_procs];
    // 참조 인덱스와 총 참조 수를 초기화
    for (int i = 0; i < num_procs; i++) {
        idx[i] = 0;
    }

    int *PF = (int *)malloc(num_procs * sizeof(int)); // 각 프로세스 별 page fault 갯수 카운트
    int *allocated_frame = (int *)malloc(num_procs * sizeof(int)); // 각 프로세스 별 할당된 frame 갯수 카운트
    int **lv1 = (int **)malloc(num_procs * sizeof(int *)); // 각 프로세스의 level 1 페이지테이블

    for (int i = 0; i < num_procs; i++) {
        PF[i] = 0;
        allocated_frame[i] = 1; // 각 프로세스 별 level1 pagetable 프레임 하나씩 할당됨
        // level1 페이지 테이블 초기화
        lv1[i] = (int *)malloc(8 * sizeof(int));
        for (int j = 0; j < 8; j++) {
            lv1[i][j] = -1;
        }
    }

    bool completed = false;
    while (!completed) {
        completed = true;
        for (int i = 0; i < num_procs; i++) {
            if (idx[i] < procs[i].ref_len) { // 아직 참조할 페이지가 남았는지 확인
                completed = false;
                int page_number = procs[i].reference[idx[i]]; // 참조할 페이지 번호 가져옴
                //printf("[PID %02d REF:%03d] Page access %03d : ", procs[i].pid, idx[i], page_number);

                int lv1_idx = page_number / 8;

                // level1 page table에 아무것도 할당되어 있지 않음
                if (lv1[i][lv1_idx] == -1) {
                    // lv1_frame 할당해줌
                    if(!check_memory(frame_num, procs, num_procs, allocated_frame, PF, lv1)){
                        return;
                    }
                    // page fault 처리
                    frame_num += 1;
                    allocated_frame[i] += 1;
                    PF[i] += 1;
                    lv1[i][lv1_idx] = frame_num;
                    //printf("(L1PT) PF, Allocated Frame %03d -> %03d, ", lv1_idx, frame_num);

                    // lv2 의 프레임을 할당해줌
                    if(!check_memory(frame_num, procs, num_procs, allocated_frame, PF, lv1)){
                        return;
                    }
                    // page fault 처리
                    page_fault(&procs[i], page_number, &frame_num, &allocated_frame[i], &PF[i]);

                    //printf("(L2PT) PF, Allocated Frame %03d\n", frame_num);
                }
                // level1 page table에 이미 할당되어 있음
                else {
                    //printf("(L1PT) Frame %03d -> %03d, ", lv1_idx, lv1[i][lv1_idx]);
                    if (procs[i].page_table[page_number].vflag == PAGE_VALID) { // 해당 프레임의 엔트리가 있는지 확인
                        //printf("(L2PT) Frame %03d\n", procs[i].page_table[page_number].frame);
                        procs[i].page_table[page_number].ref+=1;
                    } else {
                        if(!check_memory(frame_num, procs, num_procs, allocated_frame, PF, lv1)){
                        return;
                        }
                        // page fault 처리
                        page_fault(&procs[i], page_number, &frame_num, &allocated_frame[i], &PF[i]);
                        //printf("(L2PT) PF, Allocated Frame %03d\n", frame_num);
                    }
                }
                idx[i]++;
                procs[i].cnt++;
            }
        }
    }

    // 실행 후 결과 출력
    print_page_table(procs, num_procs, allocated_frame, PF, lv1);

    // 메모리 해제
    for (int i = 0; i < num_procs; i++) {
        free(lv1[i]);
    }
    free(lv1);
    free(PF);
    free(allocated_frame);
}

int main(int argc, char *argv[]) {
    FILE *fp;

    // 파일 변수 선언
    // 파일 이름이 주어지지 않은 경우 예외 처리
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

    // 프로세스 정보 읽기
    int num_procs = 0;
    int frame_num = 0;
    process procs[MAX_PROCESSES];

    while (!feof(fp)) { // 파일 끝까지 읽어들이기
        int pid, ref_len;
        if (fread(&pid, sizeof(int), 1, fp) != 1) break; // 프로세스 ID 읽기
        if (fread(&ref_len, sizeof(int), 1, fp) != 1) break; // 참조 길이 읽기
        // 참조하는 페이지 번호 읽기
        unsigned char *references = (unsigned char *)malloc(ref_len * sizeof(unsigned char));
        if (fread(references, sizeof(unsigned char), ref_len, fp) != ref_len) break;
        // 프로세스 로드
        load_process(&procs[num_procs], pid, references, ref_len);
        num_procs++;
    }

    // 프로세스 실행
    frame_num=num_procs-1;
    execute_processes(procs, num_procs, frame_num);

    // 파일 닫기
    if (fp != stdin) {
        fclose(fp);
    }

    return 0;
}