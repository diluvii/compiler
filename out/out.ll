target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

declare void @print(i32)

declare i32 @read()

define i32 @func(i32 %0) {
  %a_1 = alloca i32, align 4
  %b_2 = alloca i32, align 4
  %p_0 = alloca i32, align 4
  %return = alloca i32, align 4
  store i32 %0, ptr %p_0, align 4
  store i32 10, ptr %a_1, align 4
  %2 = load i32, ptr %a_1, align 4
  %3 = load i32, ptr %p_0, align 4
  %4 = add i32 %2, %3
  store i32 %4, ptr %b_2, align 4
  %5 = load i32, ptr %b_2, align 4
  store i32 %5, ptr %return, align 4
  br label %6

6:                                                ; preds = %1
  %7 = load i32, ptr %return, align 4
  ret i32 %7
}
