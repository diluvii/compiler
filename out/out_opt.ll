target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

declare void @print(i32)

declare i32 @read()

define i32 @func(i32 %0) {
  %a_3 = alloca i32, align 4
  %i_2 = alloca i32, align 4
  %n_0 = alloca i32, align 4
  %sum_1 = alloca i32, align 4
  %return = alloca i32, align 4
  store i32 %0, ptr %n_0, align 4
  store i32 0, ptr %i_2, align 4
  store i32 0, ptr %sum_1, align 4
  br label %4

2:                                                ; preds = %15
  %3 = load i32, ptr %return, align 4
  ret i32 %3

4:                                                ; preds = %8, %1
  %5 = load i32, ptr %i_2, align 4
  %6 = load i32, ptr %n_0, align 4
  %7 = icmp slt i32 %5, %6
  br i1 %7, label %8, label %15

8:                                                ; preds = %4
  %9 = call i32 @read()
  store i32 %9, ptr %a_3, align 4
  %10 = load i32, ptr %sum_1, align 4
  %11 = load i32, ptr %a_3, align 4
  %12 = add i32 %10, %11
  store i32 %12, ptr %sum_1, align 4
  %13 = load i32, ptr %i_2, align 4
  %14 = add i32 %13, 1
  store i32 %14, ptr %i_2, align 4
  br label %4

15:                                               ; preds = %4
  %16 = load i32, ptr %sum_1, align 4
  store i32 %16, ptr %return, align 4
  br label %2
}
