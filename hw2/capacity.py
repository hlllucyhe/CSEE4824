import matplotlib.pyplot as plt
import numpy as np

# ====== 数据 ======
capacity = np.array([16, 32, 64, 128, 256])
miss_rate = np.array([0.0210, 0.0013, 0.0005, 0.0003, 0.0004])
cold = np.array([2586, 2586, 1466, 1466, 1466])
capacity_miss = np.array([980, 104, 0, 0, 0])
mapping = np.array([48770, 2050, 514, 135, 410])
replacement = np.array([48385, 1299, 193, 10, 37])
total = np.array([100721, 6039, 2173, 1611, 1913])

# 百分比
cold_p = cold / total * 100
capacity_p = capacity_miss / total * 100
mapping_p = mapping / total * 100
replacement_p = replacement / total * 100

# ====== 柔和蓝色配色 ======
colors = {
    "cold": "#A7C7FF",        # light sky blue
    "capacity": "#6FA8FF",    # muted light blue
    "mapping": "#3C82F6",     # medium vivid blue
    "replacement": "#204E96", # dark navy blue
    "line": "#102A63"         # deep indigo for line
}

plt.style.use("seaborn-v0_8-whitegrid")

fig, ax1 = plt.subplots(figsize=(9,5), dpi=150)
bar_width = 0.18
x = np.arange(len(capacity))

# ====== 绘制分组柱形图 ======
b1 = ax1.bar(x - 1.5*bar_width, cold_p, bar_width, color=colors["cold"], label='Cold')
b2 = ax1.bar(x - 0.5*bar_width, capacity_p, bar_width, color=colors["capacity"], label='Capacity')
b3 = ax1.bar(x + 0.5*bar_width, mapping_p, bar_width, color=colors["mapping"], label='Mapping')
b4 = ax1.bar(x + 1.5*bar_width, replacement_p, bar_width, color=colors["replacement"], label='Replacement')

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

# ====== 左轴 ======
ax1.set_ylabel("Miss Type Percentage (%)", fontsize=11)
ax1.set_xlabel("Cache Capacity (KB)", fontsize=11)
ax1.set_xticks(x)
ax1.set_xticklabels(capacity, fontsize=10)
ax1.set_ylim(0, 100)
ax1.tick_params(axis="y", labelsize=9, colors="#333333")
ax1.set_yticks(np.arange(0, 101, 10))
ax1.grid(alpha=0.25, linestyle="--", linewidth=0.6)

# ====== 右轴：Miss Rate 折线 ======
ax2 = ax1.twinx()
ax2.plot(
    x, miss_rate, marker='o', markersize=6,
    color=colors["line"], linewidth=2.2, label='Miss Rate', zorder=5
)
ax2.fill_between(x, miss_rate, color=colors["line"], alpha=0.12)
ax2.set_ylabel("Miss Rate", color=colors["line"], fontsize=11)
ax2.tick_params(axis="y", colors=colors["line"], labelsize=9)
ax2.set_ylim(0, max(miss_rate)*1.6)

# ====== 折线节点标注 ======
for i, rate in enumerate(miss_rate):
    ax2.text(
        x[i], rate + max(miss_rate)*0.03,
        f"{rate:.4f}", ha='center', va='bottom',
        fontsize=8, color=colors["line"], fontweight='bold'
    )

# ====== 标题与图例 ======
fig.suptitle(
    "Miss Rate and Miss Type Composition vs Cache Capacity (64B Block, Direct-Mapped)",
    fontsize=13, fontweight="bold", color=colors["line"], y=1.05
)

# 图例
bars_labels = ["Cold", "Capacity", "Mapping", "Replacement", "Miss Rate"]
bars_colors = [colors["cold"], colors["capacity"], colors["mapping"], colors["replacement"], colors["line"]]
handles = [plt.Line2D([0],[0], color=c, lw=6) for c in bars_colors]
ax1.legend(
    handles, bars_labels, loc="upper center",
    bbox_to_anchor=(0.5, 1.12), ncol=5, frameon=False, fontsize=9
)

# ====== 美化边框与布局 ======
ax1.spines["top"].set_visible(False)
ax2.spines["top"].set_visible(False)
plt.tight_layout()
# plt.savefig("capacity_sensitivity_final.png", bbox_inches="tight", dpi=300)
plt.show()