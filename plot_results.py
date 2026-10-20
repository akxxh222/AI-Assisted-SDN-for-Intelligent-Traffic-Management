import pandas as pd
import matplotlib.pyplot as plt

df_trad = pd.read_csv("/home/ghari/ns-3.48/metrics_traditional.csv")
df_ai = pd.read_csv("/home/ghari/ns-3.48/metrics_ai.csv")

fig, axs = plt.subplots(3, 1, figsize=(10, 11), sharex=True)

# 1. Throughput Comparison
axs[0].plot(df_trad['timestamp'], df_trad['throughput_mbps'], 'r-o', label='Traditional Static Routing (Congested)')
axs[0].plot(df_ai['timestamp'], df_ai['throughput_mbps'], 'g-s', label='AI-Assisted SDN (Dynamic Proactive Reroute)')
axs[0].set_ylabel('Throughput (Mbps)', fontsize=11)
axs[0].set_title('100-Node SDN Network Performance Evaluation', fontsize=14, fontweight='bold')
axs[0].grid(True, linestyle='--', alpha=0.6)
axs[0].legend(loc='lower left')

# 2. Delay Comparison
axs[1].plot(df_trad['timestamp'], df_trad['delay_ms'], 'r-o', label='Traditional Routing (High Latency)')
axs[1].plot(df_ai['timestamp'], df_ai['delay_ms'], 'g-s', label='AI-Assisted SDN (Low Latency)')
axs[1].set_ylabel('Avg Delay (ms)', fontsize=11)
axs[1].grid(True, linestyle='--', alpha=0.6)
axs[1].legend(loc='upper left')

# 3. Packet Loss Ratio Comparison
axs[2].plot(df_trad['timestamp'], df_trad['loss_ratio'] * 100, 'r-o', label='Traditional Routing (Packet Drops)')
axs[2].plot(df_ai['timestamp'], df_ai['loss_ratio'] * 100, 'g-s', label='AI-Assisted SDN (Zero/Minimized Drops)')
axs[2].set_ylabel('Packet Loss (%)', fontsize=11)
axs[2].set_xlabel('Simulation Time (Seconds)', fontsize=12)
axs[2].grid(True, linestyle='--', alpha=0.6)
axs[2].legend(loc='upper left')

plt.tight_layout()
plt.savefig("sdn_100_node_performance.png", dpi=300)
print("\nGraph successfully saved to 'sdn_100_node_performance.png'")
