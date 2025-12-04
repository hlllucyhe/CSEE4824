import matplotlib.pyplot as plt
import numpy as np

# ====== 数据 ======
block = np.array([8, 16, 32, 64, 128])
miss_rate = np.array([0.0042, 0.0024, 0.0015, 0.0047, 0.0028])
cold = np.array([77.75, 72.30, 64.86, 11.50, 10.94])
capacity = np.array([0.75, 1.17, 1.70, 0.47, 0.70])
mapping = np.array([15.48, 20.70, 24.38, 82.20, 46.29])
replacement = np.array([6.02, 5.83, 9.07, 5.83, 41.92])


# ====== 柔和蓝色配色 ======
colors = {
    "cold": "#9EC9FF",        # very light blue
    "capacity": "#5FA3FF",    # muted medium blue
    "mapping": "#3B82F6",     # brighter blue
    "replacement": "#1E3A8A", # deep navy
    "line": "#0B215C"         # dark indigo
}

plt.style.use("seaborn-v0_8-whitegrid")

fig, ax1 = plt.subplots(figsize=(8,5), dpi=150)
bar_width = 0.18
x = np.arange(len(block))

# ====== 分组柱形图（细柱、柔和蓝色） ======
b1 = ax1.bar(x - 1.5*bar_width, cold, width=bar_width, color=colors["cold"], label='Cold')
b2 = ax1.bar(x - 0.5*bar_width, capacity, width=bar_width, color=colors["capacity"], label='Capacity')
b3 = ax1.bar(x + 0.5*bar_width, mapping, width=bar_width, color=colors["mapping"], label='Mapping')
b4 = ax1.bar(x + 1.5*bar_width, replacement, width=bar_width, color=colors["replacement"], label='Replacement')

# ====== 在柱子上方标注百分比 ======
def label_bars(bars):
    for bar in bars:
        height = bar.get_height()
        if height > 0.1:  # 过滤太小的值
            ax1.text(
                bar.get_x() + bar.get_width()/2, height + 0.8,
                f'{height:.1f}%', ha='center', va='bottom',
                fontsize=8, color='#1b1b1b'
            )
for group in [b1, b2, b3, b4]:
    label_bars(group)

# ====== 左轴设置 ======
ax1.set_ylabel("Miss Type Percentage (%)", fontsize=11)
ax1.set_xlabel("Block Size (B)", fontsize=11)
ax1.set_xticks(x)
ax1.set_xticklabels(block, fontsize=10)
ax1.set_ylim(0, 100)
ax1.tick_params(axis="y", labelsize=9, colors="#333333")

# 网格线细化（10% 间距）
ax1.set_yticks(np.arange(0, 101, 10))
ax1.grid(alpha=0.3, linestyle="--", linewidth=0.6)

# ====== 右轴：Miss Rate 折线 ======
ax2 = ax1.twinx()
ax2.plot(
    x, miss_rate, marker='o', markersize=5,
    color=colors["line"], linewidth=2.2, label='Miss Rate', zorder=5
)
ax2.fill_between(x, miss_rate, color=colors["line"], alpha=0.12)
ax2.set_ylabel("Miss Rate", color=colors["line"], fontsize=11)
ax2.tick_params(axis="y", colors=colors["line"], labelsize=9)
ax2.set_ylim(0, max(miss_rate)*1.5)

# ====== 折线节点标注 ======
for i, rate in enumerate(miss_rate):
    ax2.text(
        x[i], rate + max(miss_rate)*0.03,
        f"{rate:.4f}", ha='center', va='bottom',
        fontsize=8, color=colors["line"], fontweight='bold'
    )

# ====== 图标题与图例 ======
fig.suptitle(
    "Miss Rate and Miss Type Composition vs Block Size (32KB Direct-Mapped)",
    fontsize=13, fontweight="bold", color=colors["line"], y=1.04
)

# 图例居中、一行展示
bars_labels = ["Cold", "Capacity", "Mapping", "Replacement", "Miss Rate"]
bars_colors = [colors["cold"], colors["capacity"], colors["mapping"], colors["replacement"], colors["line"]]
handles = [plt.Line2D([0], [0], color=c, lw=6) for c in bars_colors]
ax1.legend(handles, bars_labels, loc="upper center", bbox_to_anchor=(0.5, 1.12),
            ncol=5, frameon=False, fontsize=9)

# ====== 边框与布局优化 ======
ax1.spines["top"].set_visible(False)
ax2.spines["top"].set_visible(False)
plt.tight_layout()
# plt.savefig("capacity_sensitivity_blue.png", bbox_inches="tight", dpi=300)
plt.show()