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
  br label %4

2:                                                ; preds = %17
  %3 = load i32, ptr %return, align 4
  ret i32 %3

4:                                                ; preds = %8, %1
  %5 = load i32, ptr %c_3, align 4
  %6 = load i32, ptr %n_0, align 4
  %7 = icmp slt i32 %5, %6
  br i1 %7, label %8, label %17

8:                                                ; preds = %4
  %9 = load i32, ptr %a1_1, align 4
  call void @print(i32 %9)
  %10 = load i32, ptr %c_3, align 4
  %11 = add i32 %10, 1
  store i32 %11, ptr %c_3, align 4
  %12 = load i32, ptr %a2_2, align 4
  store i32 %12, ptr %t_4, align 4
  %13 = load i32, ptr %a1_1, align 4
  %14 = load i32, ptr %a2_2, align 4
  %15 = add i32 %13, %14
  store i32 %15, ptr %a2_2, align 4
  %16 = load i32, ptr %t_4, align 4
  store i32 %16, ptr %a1_1, align 4
  br label %4

17:                                               ; preds = %4
  store i32 1, ptr %return, align 4
  br label %2
}
