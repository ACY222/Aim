// tests/test_buffer.cpp
#include "buffer.hpp"
#include <cassert>
#include <iostream>

// 测试 1：初始化状态
void test_initialization() {
    Buffer buf;
    assert(buf.getLineCount() == 1);
    assert(buf.getLineLength(0) == 0);
    assert(buf.isEmpty(0) == true);
    std::cout << "[PASS] test_initialization\n";
}

// 测试 2：基础字符插入
void test_insert_char() {
    Buffer buf;
    int cx = 0, cy = 0;

    buf.insertChar(cx, cy, 'a');
    buf.insertChar(cx, cy, 'b');

    assert(buf.getLineCount() == 1);
    assert(buf.rows[0] == "ab");
    assert(cx == 2); // 光标应该前进到 2
    std::cout << "[PASS] test_insert_char\n";
}

// 测试 3：行中回车拆分 (Split)
void test_insert_newline_split() {
    Buffer buf;
    int cx = 0, cy = 0;
    buf.rows[0] = "HelloWorld"; // 直接操作底层数据进行环境准备
    cx = 5;                     // 光标移到 "Hello" 和 "World" 之间

    buf.insertNewLine(cx, cy);

    assert(buf.getLineCount() == 2);
    assert(buf.rows[0] == "Hello");
    assert(buf.rows[1] == "World");
    assert(cy == 1); // 光标应该来到下一行
    assert(cx == 0); // 光标应该在行首
    std::cout << "[PASS] test_insert_newline_split\n";
}

// 测试 4：行首 Backspace 合并 (Merge)
void test_delete_char_merge() {
    Buffer buf;
    buf.rows[0] = "Hello";
    buf.rows.push_back("World");
    int cx = 0, cy = 1; // 光标在第二行的 'W' 上

    buf.deleteChar(cx, cy);

    assert(buf.getLineCount() == 1);
    assert(buf.rows[0] == "HelloWorld");
    assert(cy == 0); // 光标回到上一行
    assert(cx == 5); // 光标应该在 'W' 的前面，即索引 5
    std::cout << "[PASS] test_delete_char_merge\n";
}

// 测试 5：Vim 的清空行逻辑 (cc)
void test_clear_lines() {
    Buffer buf;
    buf.rows[0] = "Line 1";
    buf.rows.push_back("Line 2");
    buf.rows.push_back("Line 3");

    // 模拟 2cc (修改当前行及下一行，共2行)
    buf.clearLines(0, 2);

    assert(buf.getLineCount() == 2); // 删掉了一行，还剩两行
    assert(buf.rows[0] == "");       // 第一行被清空留作输入
    assert(buf.rows[1] == "Line 3"); // 原先的第三行提上来了
    std::cout << "[PASS] test_clear_lines\n";
}

int main() {
    std::cout << "--- Running Buffer Unit Tests ---\n";

    test_initialization();
    test_insert_char();
    test_insert_newline_split();
    test_delete_char_merge();
    test_clear_lines();

    std::cout << "--- All Tests Passed Successfully! ---\n";
    return 0;
}
