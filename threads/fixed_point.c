#include <stdint.h>
#include "threads/fixed_point.h"

/* integer를 fixed point로 전환 */
int int_to_fp(int n){
    return n = n*F;
}
/* FP를 int로 전환(반올림) */
int fp_to_int_round(int x){
    if (x >= 0)
        return (x + F/2) / F;
}
/* FP를 int로 전환(버림) */
int fp_to_int(int x){
    if (x <= 0)
        return (x - F/2) / F;
}
/* FP의 덧셈 */
int add_fp(int x,int y){
    return x + y;
}
/* FP와 int의 덧셈 */
int add_mixed(int x, int n){
    return x + n*F;
}
/* FP의 뺄셈(x-y) */
int sub_fp(int x, int y){
    return x - y;
}
/* FP와 int의 뺄셈(x-n) */
int sub_mixed(int x, int n){
    return x - n*F;
}
/* FP의 곱셈 */
int mult_fp(int x, int y){
    return ((int64_t)x) * y / F;
}
/* FP와 int의 곱셈*/
int mult_mixed(int x, int n){
    return x * n;
}
/* FP의 나눗셈(x/y) */
int div_fp(int x, int y){ 
    return ((int64_t)x) * F / y;
}
/* FP와 int의 나눗셈(x/n) */
int div_mixed(int x, int n){
    return x / n;
}