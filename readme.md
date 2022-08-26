# 正则表达式编译到 C！

支持 ERE 哦

## 构建说明

需要一个支持 c99 的编译器 :D。

```
make
```

然后就可以收获 `target/regex-to-c` 了。

## 使用说明

### 获取编译结果

regex-to-c 仅需要一个参数，是待编译的正则表达式。然后她会把编译结果输出到标准输出，是一份 C 代码。

比如：

```
./target/regex-to-c '(ne.er|gon+a|giv*|you(up))'
```

……然后就会获得一份长达 1112 行的巨大 C 代码。

### 使用编译结果

得到的巨大 C 代码中，会有一个 `match()` 函数作为接口。该函数原型为

```
int match(int *len, int *success, char *str);
```

该函数主要功能为，判断给定的字符串 `str`，是否有一个前缀能够与编译好的正则表达式匹配。如果存在这样一个前缀，则返回 `1`，否则返回 `0`。

调用 `match()` 时，需要提供三个参数。

1. 如果 `len` 和 `success` 两者之一为 `NULL`，则 `match()` 不会使用 `len` 和 `success` 两个参数。仅仅依靠返回值来表示匹配结果。

2. 如果 `len` 和 `success` 均不为 `NULL`，则 `match()` 会将匹配结果保存在这两个指针所指向的变量中。其中，`len` 所指向的变量将会被修改为匹配前缀的长度； `success` 所指向的变量会被修改为匹配的结果：如果 `str` 中存在前缀能被编译好的正则表达式匹配，该变量会被修改为 `1`，否则修改为 `0`。

一个 `match()` 函数的示例如下：

```c
// 前面粘贴一堆编译产出

int main(int argc, char *argv[]) {
    if (match(&len, &success, argv[1])) {
        printf("match for %d chars\n", len);
    } else {
        printf("not match\n");
    }
    return 0;
}
```

该程序尝试用编译好的正则表达式匹配第一个命令行变量：如果匹配，则输出匹配的字节数；否则，输出提示信息。

## 文档编写中……
