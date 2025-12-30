#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
#define NKEYS 100000

struct entry {
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];
int keys[NKEYS];
int nthread = 1;

pthread_mutex_t locks[NBUCKET];//每个桶一个锁

/// @brief 获取当前时间（秒）
/// @return 当前时间（秒）
double
now()
{
 struct timeval tv;
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void 
insert(int key, int value, struct entry **p, struct entry *n)
{
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;
}

/// @brief 插入或更新键值对(对桶里装的链表操作) 就是建表
/// @param key 
/// @param value 
static 
void put(int key, int value)
{
  int i = key % NBUCKET;//根据key计算hash桶的位置

  // is the key already present?
  struct entry *e = 0;

   pthread_mutex_lock(&locks[i]); // 加bucket锁
  //先在 table[i] 这条链表里找 key 是否已存在
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  //找到了就更新；没找到就插到链表头部
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    insert(key, value, &table[i], table[i]);
  }
   pthread_mutex_unlock(&locks[i]); // 放bucket锁

}


/// @brief 根据key查找对应的entry(桶的位置+链表遍历)
/// @param key 
/// @return entry* or NULL if not found（找到的就是key value对）
static struct entry*
get(int key)
{
  int i = key % NBUCKET;


  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }

  return e;
}

/// @brief 线程函数：插入键值对
/// @param xa 
/// @return 
static void *
put_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int b = NKEYS/nthread;

  for (int i = 0; i < b; i++) {
    put(keys[b*n + i], n);
  }

  return NULL;
}

static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }

  for (int i = 0; i < NBUCKET; i++) {
    pthread_mutex_init(&locks[i], NULL);
  }

  // number of threads
  nthread = atoi(argv[1]);
  //你要开 nthread 个线程，就需要一个数组把它们的 handle 存起来，后面 join 才能用。
  //tha 是个指针，指向一块动态分配的内存
  //这块内存用来存 nthread 个 pthread_t（每个线程一个“线程句柄/ID”）
  tha = malloc(sizeof(pthread_t) * nthread);
  //它是给随机数生成器设种子（seed）。
  srandom(0);
  //如果条件为真：什么也不发生，继续运行(assert函数的作用)
  //如果条件为假：程序立刻报错并退出（通常打印出失败位置）
  assert(NKEYS % nthread == 0);//让NKEYS能够处理nthread个线程 就是分配工作 把NKEYS均分给nthread个线程去建表 查找之类的
  for (int i = 0; i < NKEYS; i++) {
    // generate random keys 给输入的key赋随机值
    keys[i] = random();
  }

  //
  // first the puts
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    /*&tha[i]：把新线程的句柄写到 tha[i] 里
    NULL：线程属性用默认值
    put_thread：线程启动后执行的函数
    (void*)(long)i：传给线程函数的参数（只能传一个 void*）
    assert 确保线程创建成功*/
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  for (int i = 0; i < nthread; i++)
  {
    /*内核把 main 线程标记为 sleeping / blocked（不可运行）
      main 从 runnable 队列里移除
      调度器只能在剩下 runnable 的线程里挑一个运行（通常就是你创建的那些线程）*/
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
