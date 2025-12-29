#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

/*
 * strcpy - 将以 NUL 结尾的字符串从 `t` 复制到 `s`。
 * @s: 目标缓冲区（须足够大）
 * @t: 源 NUL 终止字符串
 * 返回: 指向目标缓冲区 `s` 的指针。
 */
char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

/*
 * strcmp - 逐字典序比较两个 NUL 终止的字符串。
 * @p: 第一个字符串
 * @q: 第二个字符串
 * 返回: 若 p<q 返回负值，p==q 返回 0，p>q 返回正值（按无符号字符差值）。
 */
int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

/*
 * strlen - 计算 NUL 终止字符串的长度。
 * @s: 输入字符串
 * 返回: 不包括终止 NUL 的字符数。
 */
uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

/*
 * memset - 用指定字节值填充一段内存区域。
 * @dst: 目标内存区域
 * @c: 字节值（会转换为 unsigned char）
 * @n: 要设置的字节数
 * 返回: 指向 `dst` 的指针。
 */
void*
memset(void *dst, int c, uint n)
{
  char *cdst = (char *) dst;
  int i;
  for(i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

/*
 * strchr - 在字符串 `s` 中查找字符 `c` 的首次出现。
 * @s: 要搜索的 NUL 终止字符串
 * @c: 要查找的字符
 * 返回: 指向匹配字符的指针，未找到返回 0。
 */
char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

/*
 * gets - 从文件描述符 0（标准输入）读取一行到缓冲区。
 * @buf: 目标缓冲区
 * @max: 最大读取字节数（包含终止 NUL）
 * 说明: 最多读取 max-1 字节，遇到 EOF 或换行/回车则停止。
 * 返回: 指向 `buf` 的指针（已 NUL 终止）。
 */
char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

/*
 * stat - 获取指定文件的状态信息。
 * @n: 文件路径名
 * @st: 指向将被填充的 struct stat
 * 返回: 成功返回 0，失败返回 -1。
 * 说明: 以只读方式打开文件，调用 fstat，然后关闭文件。
 */
int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

/*
 * atoi - 将数字字符串转换为整数。
 * @s: 包含十进制数字的 NUL 终止字符串
 * 返回: 解析得到的非负整数（遇到第一个非数字字符停止）。
 */
int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

/*
 * memmove - 将 `n` 字节从 `vsrc` 复制到 `vdst`，可正确处理重叠区域。
 * @vdst: 目标缓冲区
 * @vsrc: 源缓冲区
 * @n: 要复制的字节数
 * 返回: 指向 `vdst` 的指针。
 */
void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  if (src > dst) {
    while(n-- > 0)
      *dst++ = *src++;
  } else {
    dst += n;
    src += n;
    while(n-- > 0)
      *--dst = *--src;
  }
  return vdst;
}

/*
 * memcmp - 按字节比较两段内存区域。
 * @s1: 第一段内存
 * @s2: 第二段内存
 * @n: 比较的字节数
 * 返回: 相等返回 0，若在首个不同字节处 s1<s2 返回负值，s1>s2 返回正值（按无符号字符差值）。
 */
int
memcmp(const void *s1, const void *s2, uint n)
{
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

/*
 * memcpy - 将 `n` 字节从 `src` 复制到 `dst`。
 * @dst: 目标缓冲区
 * @src: 源缓冲区
 * @n: 要复制的字节数
 * 返回: 指向 `dst` 的指针。
 * 说明: 通过 memmove 实现，能够正确处理重叠情况。
 */
void *
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}
