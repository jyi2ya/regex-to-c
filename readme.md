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
int match(char *str);
```

该函数主要功能为，判断给定的字符串 `str`，是否有一个前缀能够与编译好的正则表达式匹配。如果存在这样一个前缀，则返回该前缀的长度，否则返回 `0`。

一个 `match()` 函数的示例如下：

```c
// 前面粘贴一堆编译产出

int main(int argc, char *argv[]) {
    int len = match(argv[1]));
    if (len != 0) {
        printf("match for %d chars\n", len);
    } else {
        printf("not match\n");
    }
    return 0;
}
```

该程序尝试用编译好的正则表达式匹配第一个命令行变量：如果匹配，则输出匹配的字节数；否则，输出提示信息。

## 文档编写中……
