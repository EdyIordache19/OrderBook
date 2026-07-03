from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns


def load_latency_data(csv_path):
    print("Loading chronological latency data...")
    df = pd.read_csv(csv_path, skipinitialspace=True)
    df.columns = df.columns.str.strip()

    required = ["order_id", "engine_latency", "core_to_core_latency"]
    missing = [column for column in required if column not in df.columns]
    if missing:
        raise ValueError(f"Missing required columns: {missing}")

    df = df[required].copy()
    for column in required:
        df[column] = pd.to_numeric(df[column], errors="coerce")

    df = df.dropna().sort_values("order_id").reset_index(drop=True)
    return df


def summarize_blocks(df, column, block_size):
    block_id = np.arange(len(df)) // block_size
    summary = df.groupby(block_id).agg(
        order_start=("order_id", "min"),
        order_end=("order_id", "max"),
        order_center=("order_id", "mean"),
        p50=(column, "median"),
        p99=(column, lambda values: np.percentile(values, 99)),
    )
    return summary


def format_latency_stats(series):
    median = series.median()
    p99 = np.percentile(series, 99)
    p999 = np.percentile(series, 99.9)
    max_value = series.max()
    return {
        "median": median,
        "p99": p99,
        "p999": p999,
        "max": max_value,
        "p99_over_median": p99 / median,
        "p999_over_median": p999 / median,
    }


def plot_latency_regime(df, column, title, color_scale, rolling_colors, output_path, xlabel=False):
    stats = format_latency_stats(df[column])
    block_size = max(5000, len(df) // 200)
    blocks = summarize_blocks(df, column, block_size)
    display_max_y = stats["p999"] * 1.25

    sns.set_theme(style="whitegrid", context="talk")
    fig, ax = plt.subplots(figsize=(16, 8))

    ax.hexbin(
        df["order_id"],
        df[column],
        extent=(0, df["order_id"].max(), 0, display_max_y),
        gridsize=150,
        mincnt=1,
        bins="log",
        cmap=color_scale,
        rasterized=True,
        linewidths=0,
        edgecolors="none",
        alpha=0.96,
    )
    ax.plot(blocks["order_center"], blocks["p50"], color=rolling_colors[0], linewidth=1.8, label="Rolling p50")
    ax.plot(blocks["order_center"], blocks["p99"], color=rolling_colors[1], linewidth=1.8, linestyle="--", label="Rolling p99")
    ax.axhline(stats["median"], color=rolling_colors[0], linewidth=1.0, alpha=0.8)
    ax.axhline(stats["p99"], color=rolling_colors[1], linewidth=1.0, linestyle="--", alpha=0.8)
    ax.axhline(stats["p999"], color=rolling_colors[2], linewidth=1.0, linestyle=":", alpha=0.9)
    ax.set_title(title, fontsize=16, fontweight="bold")
    ax.set_ylabel("Latency (ns)")
    if xlabel:
        ax.set_xlabel("Order ID")
    else:
        ax.set_xlabel("")
        ax.tick_params(labelbottom=False)
    ax.set_ylim(0, display_max_y)
    ax.legend(loc="upper right", frameon=True, fontsize=10)
    ax.text(
        0.01,
        0.96,
        f"median {stats['median']:.0f} ns | p99 {stats['p99']:.0f} ns | p99.9 {stats['p999']:.0f} ns",
        transform=ax.transAxes,
        va="top",
        ha="left",
        fontsize=10,
        bbox=dict(facecolor="white", edgecolor="none", alpha=0.85),
    )
    sns.despine(fig=fig, left=False, bottom=False)

    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    print(f"Exported high-resolution chart to {output_path}")


def plot_tail_survival(df, output_path):
    fig, ax = plt.subplots(figsize=(12, 8))

    sns.ecdfplot(df["engine_latency"], ax=ax, complementary=True, color="#2c3e50", linewidth=2.2, label="Engine")
    sns.ecdfplot(df["core_to_core_latency"], ax=ax, complementary=True, color="#2980b9", linewidth=2.2, label="Core-to-core")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_title("Tail survival curve", fontsize=16, fontweight="bold")
    ax.set_xlabel("Latency (ns, log scale)")
    ax.set_ylabel("P(X >= x)")
    ax.legend(frameon=True, fontsize=10)
    ax.grid(True, which="both", alpha=0.2)
    sns.despine(fig=fig, left=False, bottom=False)

    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    print(f"Exported high-resolution chart to {output_path}")


def generate_latency_profiles(csv_path, output_dir=None):
    df = load_latency_data(csv_path)

    if output_dir is None:
        output_dir = Path(__file__).resolve().parent
    else:
        output_dir = Path(output_dir)

    plot_latency_regime(
        df,
        column="engine_latency",
        title="Engine latency regime over time",
        color_scale="viridis",
        rolling_colors=("#2ecc71", "#ff9f43", "#eb4d4b"),
        output_path=output_dir / "engine_latency_profile.png",
        xlabel=False,
    )
    plot_latency_regime(
        df,
        column="core_to_core_latency",
        title="Core-to-core latency regime over time",
        color_scale="magma",
        rolling_colors=("#3498db", "#9b59b6", "#e74c3c"),
        output_path=output_dir / "core_to_core_latency_profile.png",
        xlabel=True,
    )
    plot_tail_survival(df, output_dir / "latency_tail_survival.png")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    generate_latency_profiles(root / "docs" / "latencies.csv", root / "docs")