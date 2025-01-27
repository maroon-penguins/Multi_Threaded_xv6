// #include"types.h"
// #include"user.h"
// #include"stat.h"
// Lock My_Lock;
// void function(void* arg1,void* arg2){
//     int* X=(int*)arg2;
//     REQUEST(3);
//     Lock_Acquire(&My_Lock);
//     printf(2,"Thread %d Finished with value =%d\n",(*X),2*(*X)+1);
//     Lock_Release(&My_Lock);
//     exit();
// }
// int main(){
//     void* x=0;
//     printf(0,"TEST : NEW SYSCALLS %d %d %d %d", READ(3,4,4,x),WRITE(3,x,4,4),REQUEST(3),RELEASE(3));   
//     int l=3;
//     int* size=&l;
//     int list[3];
//     printf(0,"***This Program will calculate 2x+1 for 3 threads where x is the tid passed to thread as its 2nd arg***\n");
//     Lock_Init(&My_Lock);
//     for(int i=0;i<3;i++){
//         list[i]=i+1;
//         thread_create(&function,(void*)size,(void*)&list[i]);
//     }
//     for(int i=1;i<=3;i++){
//         join(i);
//     }
//     exit();
// }





/////////////////////////////////////////////////////////////////////


#include "types.h"
#include "user.h"
#include "stat.h"

void f1(void* arg1, void* arg2) {
    if (requestresource(0) < 0)
    {
        cprintf("Failes\n");
    } else {
        cprintf("T1 gets resource 0\n");
        int dummyCnt1 = 0;
        while (++dummyCnt1 < 10000000)
            continue;
    }
    cprintf("T1 requesting for resource 1\n");
    if (requestresource(1) < 0) {
        cprintf("Failed\n");
    } else {
        cprintf("T1 gets resource 1\n");
        int dummyCnt2 = 0;
        while (++dummyCnt2 < 10000000)
            continue;
    }
    exit();
}

void f2(void* arg1, void* arg2) {

    if (requestresource(1) < 0)
    {
        cprintf("Failed\n");
    } else {
        cprintf("T2 gets resource 1\n");
        int dummyCnt1 = 0;
        while (++dummyCnt1 < 10000000)
            continue;
        
    }
    printf(1, "T2 requests for resource 0\n");
    if (requestresource(0) < 0) {
        cprintf("failed!\n");
    } else {
        printf(1, "T2 gets resource 0\n");
        int dummyCnt2 = 0;
        while (++dummyCnt2 < 10000000)
            continue;
    }
    exit();
}


int main() {
    thread_create(&f1, 0, 0);
    thread_create(&f2, 0, 0);
    
    join(1);
    join(2);


    exit();
}
