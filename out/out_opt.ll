target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

declare void @print(i32)

declare i32 @read()

define i32 @func(i32 %0) {
  %a1_1 = alloca i32, align 4
  %a2_2 = alloca i32, align 4
  %c_3 = alloca i32, align 4
  %n_0 = alloca i32, align 4
  %t_4 = alloca i32, align 4
  %return = alloca i32, align 4
  store i32 %0, ptr %n_0, align 4
  store i32 1, ptr %a1_1, align 4
  store i32 1, ptr %a2_2, align 4
  store i32 0, ptr %c_3, align 4
  br label %3

2:                                                ; preds = %14
  ret i32 1

3:                                                ; preds = %7, %1
  %4 = load i32, ptr %c_3, align 4
  %5 = load i32, ptr %n_0, align 4
  %6 = icmp slt i32 %4, %5
  br i1 %6, label %7, label %14

7:                                                ; preds = %3
  %8 = load i32, ptr %a1_1, align 4
  call void @print(i32 %8)
  %9 = load i32, ptr %c_3, align 4
  %10 = add i32 %9, 1
  store i32 %10, ptr %c_3, align 4
  %11 = load i32, ptr %a2_2, align 4
  store i32 %11, ptr %t_4, align 4
  %12 = add i32 %8, %11
  store i32 %12, ptr %a2_2, align 4
  %13 = load i32, ptr %t_4, align 4
  store i32 %13, ptr %a1_1, align 4
  br label %3

14:                                               ; preds = %3
  store i32 1, ptr %return, align 4
  br label %2
}
