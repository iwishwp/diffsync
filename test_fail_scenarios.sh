#!/bin/bash
# 测试文件传输失败场景

set -e

echo "========================================="
echo "  diffsync 传输失败测试场景"
echo "========================================="
echo ""

# 检查是否在 git 仓库中
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo "错误：请在 git 仓库中运行此脚本"
    exit 1
fi

# 创建测试目录
TEST_DIR="/tmp/diffsync_fail_test"
REMOTE_DIR="$TEST_DIR/remote"
LOCAL_DIR="$TEST_DIR/local"

echo "📁 创建测试目录..."
rm -rf "$TEST_DIR"
mkdir -p "$REMOTE_DIR"
mkdir -p "$LOCAL_DIR"

# 创建一些测试文件
echo "📝 创建测试文件..."
cd "$LOCAL_DIR"
echo "test file 1" > file1.txt
echo "test file 2" > file2.txt
mkdir subdir
echo "test file 3" > subdir/file3.txt

echo "✅ 测试文件创建完成"
echo ""

#===========================================================
# 场景 1: 权限错误 - 创建只读目标目录
#===========================================================
echo "========================================="
echo "  场景 1: 权限错误 (只读目录)"
echo "========================================="
echo ""

# 创建一个只读目录
READONLY_DIR="$REMOTE_DIR/readonly"
mkdir -p "$READONLY_DIR"
chmod 000 "$READONLY_DIR"

echo "测试步骤："
echo "  1. 启动 diffsync: cd $LOCAL_DIR && ./build/diffsync tui"
echo "  2. 按 C 打开配置，添加测试配置："
echo "     Name: test-readonly"
echo "     Host: localhost"
echo "     User: $USER"
echo "     Path: $READONLY_DIR"
echo "  3. 选择所有文件 (按 F2)"
echo "  4. 按 Enter 同步 → 应该会失败"
echo "  5. 按 L 查看传输日志 → 应该显示失败状态 ✗"
echo "  6. 按 Esc 返回"
echo ""
echo "预期结果："
echo "  - 同步失败（权限被拒绝）"
echo "  - 日志显示文件状态为 ✗ Failed"
echo "  - 可以看到错误信息"
echo ""

#===========================================================
# 场景 2: 路径不存在
#===========================================================
echo "========================================="
echo "  场景 2: 目标路径不存在"
echo "========================================="
echo ""

echo "测试步骤："
echo "  1. 按 C 打开配置，添加新配置："
echo "     Name: test-nopath"
echo "     Host: localhost"
echo "     User: $USER"
echo "     Path: $REMOTE_DIR/nonexistent/path"
echo "  2. 选择一个文件"
echo "  3. 按 Enter 同步 → 应该会失败"
echo "  4. 按 L 查看日志"
echo ""
echo "预期结果："
echo "  - 同步失败（路径不存在）"
echo "  - 错误信息包含 'No such file or directory'"
echo ""

#===========================================================
# 场景 3: 端口错误
#===========================================================
echo "========================================="
echo "  场景 3: 连接端口错误"
echo "========================================="
echo ""

echo "测试步骤："
echo "  1. 按 C 打开配置，添加新配置："
echo "     Name: test-badport"
echo "     Host: localhost"
echo "     User: $USER"
echo "     Port: 99999  (不存在的端口)"
echo "     Path: $REMOTE_DIR"
echo "  2. 选择一个文件"
echo "  3. 按 Enter 同步 → 应该会失败"
echo "  4. 按 L 查看日志"
echo ""
echo "预期结果："
echo "  - 同步失败（连接被拒绝）"
echo "  - 错误信息包含 'Connection refused'"
echo ""

#===========================================================
# 场景 4: 测试重试功能
#===========================================================
echo "========================================="
echo "  场景 4: 测试重试功能"
echo "========================================="
echo ""

# 创建一个可写的目录
RETRY_DIR="$REMOTE_DIR/retry"
mkdir -p "$RETRY_DIR"

echo "测试步骤："
echo "  1. 首先创建一个会导致失败的配置（使用场景1的方法）"
echo "  2. 同步失败后，按 L 查看日志"
echo "  3. 在日志窗口中，失败的文件会用 ✗ 标记"
echo "  4. 选择一个失败的文件（用 ↑/↓ 导航）"
echo "  5. 按 R 重试选中的文件（或按 A 重试所有失败文件）"
echo "  6. 修改配置为正确的路径: $RETRY_DIR"
echo "  7. 观察重试是否成功"
echo ""
echo "预期结果："
echo "  - 日志窗口显示文件列表"
echo "  - 成功: ✓ file.txt"
echo "  - 失败: ✗ file2.txt"
echo "  - 跳过: ○ file3.txt"
echo "  - 按 R 可以重试单个失败文件"
echo "  - 按 A 可以重试所有失败文件"
echo "  - 重试成功后，状态从 ✗ 变成 ✓"
echo ""

#===========================================================
# 快速测试命令
#===========================================================
echo "========================================="
echo "  快速测试命令"
echo "========================================="
echo ""

echo "快速场景测试："
echo ""
echo "1️⃣  权限错误测试："
echo "   cd $LOCAL_DIR"
echo "   mkdir -p ../remote/readonly"
echo "   chmod 000 ../remote/readonly"
echo "   ./build/diffsync tui"
echo ""
echo "2️⃣  路径不存在测试："
echo "   ./build/diffsync tui"
echo "   # 配置使用路径: /tmp/test_nonexistent_$$/path"
echo ""
echo "3️⃣  端口错误测试："
echo "   ./build/diffsync tui"
echo "   # 配置使用端口: 99999"
echo ""

# 保存测试配置到文件
CONFIG_FILE="$LOCAL_DIR/test_configs.txt"
cat > "$CONFIG_FILE" << 'EOF'
# 测试配置 - 用于测试传输失败场景

# 配置 1: 权限错误
[test-readonly]
host = localhost
user = REPLACE_USER
path = /tmp/diffsync_fail_test/remote/readonly

# 配置 2: 路径不存在
[test-nopath]
host = localhost
user = REPLACE_USER
path = /tmp/diffsync_fail_test/nonexistent/path

# 配置 3: 端口错误
[test-badport]
host = localhost
user = REPLACE_USER
port = 99999
path = /tmp/diffsync_fail_test/remote

# 配置 4: 正常配置（用于重试测试）
[test-normal]
host = localhost
user = REPLACE_USER
path = /tmp/diffsync_fail_test/remote/retry
EOF

# 替换用户名
sed -i.bak "s/REPLACE_USER/$USER/g" "$CONFIG_FILE"
rm -f "$CONFIG_FILE.bak"

echo "========================================="
echo "  测试配置已保存到: $CONFIG_FILE"
echo "========================================="
echo ""

echo "测试目录信息："
echo "  本地文件: $LOCAL_DIR/"
echo "  远程目录: $REMOTE_DIR/"
echo "  测试配置: $CONFIG_FILE"
echo ""

echo "开始测试吧！运行: cd $LOCAL_DIR && ../../build/diffsync tui"
echo ""

# 自动清理提示
echo "========================================="
echo "  清理命令"
echo "========================================="
echo ""
echo "测试完成后，运行以下命令清理："
echo "  rm -rf $TEST_DIR"
echo ""
