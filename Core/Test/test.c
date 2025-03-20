#include "test.h"
// 暴露内部结构用于测试
// 辅助函数：遍历空闲链表并打印信息
void print_free_list() {
    printf("Free List:\n");
    heap_node *node = heap.head.next;
    while (node != heap.tail) {
        printf("  Node @%p: size=%zu, next=%p\n",
               (void*)node, node->node_size, (void*)node->next);
        node = node->next;
    }
    printf("Total heap size: %zu\n", heap.heap_size);
}

// 测试初始化是否正确
void test_init() {
    heap_init();
    // 验证初始空闲节点
    heap_node *first = heap.head.next;
    assert(first != NULL);
    assert(first->next == heap.tail);

    // 验证堆大小计算正确（考虑对齐）
    uintptr_t start = (uintptr_t)heap_mem;
    if (start % 8 != 0) {
        start = (start + 8) & ~7;
    }
    uintptr_t end = start + heap.heap_size;
    assert(end == (uintptr_t)heap.tail);

    printf("test_init passed\n");
}

// 测试基本分配与释放
void test_basic_alloc_free() {
    heap_init();

    // 分配小内存
    void *ptr = halloc(16);
    assert(ptr != NULL);
    assert((uintptr_t)ptr % 8 == 0);

    // 分配后堆大小减少
    size_t allocated_size = ((heap_node*)((uint8_t*)ptr - NODE_STRUCT_SIZE))->node_size;
    assert(heap.heap_size == configHeapSize - allocated_size);

    // 释放内存
    hfree(ptr);
    assert(heap.heap_size == configHeapSize);

    printf("test_basic_alloc_free passed\n");
}

// 测试内存合并
void test_coalescing() {
    heap_init();

    void *a = halloc(32);
    void *b = halloc(32);
    void *c = halloc(32);

    // 释放中间块
    hfree(b);
    // 释放前两个块
    hfree(a);

    // 现在a和b应该合并
    heap_node *node = heap.head.next;
    assert(node->node_size >= 64 + NODE_STRUCT_SIZE);

    // 释放最后一个块
    hfree(c);
    // 整个堆应合并为单个块
    assert(heap.head.next->node_size == configHeapSize);
    assert(heap.head.next->next == heap.tail);

    printf("test_coalescing passed\n");
}

// 测试对齐要求
void test_alignment() {
    heap_init();

    // 测试不同大小的对齐
    for (size_t size = 1; size <= 64; size++) {
        void *p = halloc(size);
        assert(p != NULL);
        assert((uintptr_t)p % 8 == 0);
        hfree(p);
    }

    printf("test_alignment passed\n");
}

// 测试分配失败场景
void test_alloc_failure() {
    heap_init();

    // 尝试分配整个堆空间（考虑元数据开销）
    size_t max_alloc = configHeapSize - NODE_STRUCT_SIZE*2;
    void *big = halloc(max_alloc);
    assert(big != NULL);

    // 再次分配应该失败
    void *failed = halloc(1);
    assert(failed == NULL);

    hfree(big);

    printf("test_alloc_failure passed\n");
}

// 测试最小节点分割
void test_min_node_split() {
    heap_init();

    // 计算刚好不触发分割的大小
    size_t test_size = configHeapSize - NODE_STRUCT_SIZE*3 - MIN_NODE_SIZE + 1;
    void *p = halloc(test_size);
    assert(p != NULL);

    // 检查空闲链表状态
    assert(heap.head.next->next == heap.tail); // 应该只剩一个尾节点

    hfree(p);
    assert(heap.head.next->node_size == configHeapSize); // 完全合并

    printf("test_min_node_split passed\n");
}

void mem_test(){
    test_init();
    test_basic_alloc_free();
    test_coalescing();
    test_alignment();
    test_alloc_failure();
    test_min_node_split();

}

int main() {
   return 0;
}
