# 正则表达式编译到 C！

支持 ERE 哦

## 构建说明

需要一个支持 c99 的编译器 :D。

```
make
```

然后就可以收获 `target/regex-to-c` 了。

## 使用说明

regex-to-c 仅需要一个参数，是待编译的正则表达式。然后她会把编译结果输出到标准输出，是一份 C 代码。

比如：

```
./target/regex-to-c '(ne.er|gon+a|giv*|you(up))'
```

……然后就会获得一份长达 1112 行的巨大 C 代码。

## 文档编写中……
