import matplotlib.pyplot as plt

# 数据
x_labels = ['1', '2', '3', '4', '5', '6', '1001']
y_values = [38, 3.8, 2.4, 1.93, 1.68, 1.52, 0.93]

# 创建折线图
plt.plot(x_labels, y_values, marker='o')

# 设置标题和标签
plt.title('折线图示例')
plt.xlabel('类别')
plt.ylabel('数值')

# 显示网格
plt.grid(True)

# 显示图形
plt.show()
