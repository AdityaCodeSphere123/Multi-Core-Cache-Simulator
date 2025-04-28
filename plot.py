#!/usr/bin/env python3
"""
Generates performance graphs for multicore cache simulator experiments.
"""

import subprocess
import re
import os
import sys

# Optional imports with fallback
try:
    import pandas as pd
    import matplotlib.pyplot as plt
    PLOTTING_AVAILABLE = True
except ImportError:
    PLOTTING_AVAILABLE = False
    print("Warning: pandas/matplotlib not found. CSV output only.")


class CacheExperiment:
    """Manages cache simulation experiments and result collection."""
    
    def __init__(self, simulator_path, trace_name):
        self.simulator = simulator_path
        self.trace = trace_name
        self.collected_data = []
        
    def execute_sim(self, set_bits, ways, block_bits, label="default"):
        """
        Run a single simulation with given parameters.
        Returns dict with results or None on failure.
        """
        cache_bytes = (1 << set_bits) * ways * (1 << block_bits)
        
        args = [
            self.simulator,
            "-t", self.trace,
            "-s", str(set_bits),
            "-E", str(ways),
            "-b", str(block_bits)
        ]
        
        print(f"  [{label}] s={set_bits}, E={ways}, b={block_bits} -> {cache_bytes} bytes")
        
        try:
            proc = subprocess.run(args, capture_output=True, text=True, timeout=300)
            
            if proc.returncode != 0:
                print(f"    ERROR: Simulation returned {proc.returncode}")
                return None
            
            # Parse execution time from output
            time_match = re.search(r"Maximum Execution Time.*?(\d+)", proc.stdout)
            if time_match is None:
                print("    ERROR: Could not parse execution time")
                return None
            
            exec_time = int(time_match.group(1))
            
            return {
                "experiment": label,
                "s": set_bits,
                "E": ways,
                "b": block_bits,
                "cache_size": cache_bytes,
                "exec_cycles": exec_time
            }
            
        except subprocess.TimeoutExpired:
            print("    ERROR: Simulation timed out")
            return None
        except Exception as err:
            print(f"    ERROR: {err}")
            return None
    
    def add_result(self, data):
        """Store a simulation result."""
        if data is not None:
            self.collected_data.append(data)
    
    def get_dataframe(self):
        """Convert results to pandas DataFrame."""
        if not PLOTTING_AVAILABLE:
            return None
        return pd.DataFrame(self.collected_data)
    
    def export_csv(self, filename):
        """Save results to CSV file."""
        if not self.collected_data:
            print("No data to export!")
            return False
        
        if PLOTTING_AVAILABLE:
            df = pd.DataFrame(self.collected_data)
            df.to_csv(filename, index=False)
        else:
            # Manual CSV writing
            with open(filename, 'w') as f:
                headers = list(self.collected_data[0].keys())
                f.write(",".join(headers) + "\n")
                for row in self.collected_data:
                    f.write(",".join(str(row[h]) for h in headers) + "\n")
        
        print(f"Results exported to {filename}")
        return True


class GraphGenerator:
    """Creates visualization plots from experiment data."""
    
    def __init__(self, output_folder):
        self.out_dir = output_folder
        os.makedirs(output_folder, exist_ok=True)
    
    def create_line_chart(self, df, x_col, y_col, title, xlabel, ylabel, filename, filter_exp=None):
        """Generate a simple line plot."""
        if not PLOTTING_AVAILABLE:
            return
        
        data = df[df['experiment'] == filter_exp] if filter_exp else df
        
        fig, ax = plt.subplots(figsize=(9, 6))
        ax.plot(data[x_col], data[y_col], 'b-o', linewidth=1.5, markersize=5)
        ax.set_xlabel(xlabel, fontsize=11)
        ax.set_ylabel(ylabel, fontsize=11)
        ax.set_title(title, fontsize=12, fontweight='bold')
        ax.grid(True, alpha=0.4)
        
        path = os.path.join(self.out_dir, filename)
        fig.savefig(path, dpi=120, bbox_inches='tight')
        plt.close(fig)
        print(f"  Saved: {filename}")
    
    def create_comparison_chart(self, df, experiments, x_col, y_col, title, xlabel, ylabel, filename):
        """Generate multi-line comparison plot."""
        if not PLOTTING_AVAILABLE:
            return
        
        markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p']
        colors = ['#2196F3', '#4CAF50', '#FF5722', '#9C27B0', '#FFC107']
        
        fig, ax = plt.subplots(figsize=(10, 6))
        
        idx = 0
        while idx < len(experiments):
            exp = experiments[idx]
            subset = df[df['experiment'] == exp]
            if len(subset) > 0:
                ax.plot(subset[x_col], subset[y_col], 
                       marker=markers[idx % len(markers)],
                       color=colors[idx % len(colors)],
                       linewidth=1.5, markersize=5, label=exp)
            idx += 1
        
        ax.set_xlabel(xlabel, fontsize=11)
        ax.set_ylabel(ylabel, fontsize=11)
        ax.set_title(title, fontsize=12, fontweight='bold')
        ax.legend(loc='best', fontsize=9)
        ax.grid(True, alpha=0.4)
        
        path = os.path.join(self.out_dir, filename)
        fig.savefig(path, dpi=120, bbox_inches='tight')
        plt.close(fig)
        print(f"  Saved: {filename}")


def validate_setup(sim_exe, trace_prefix):
    """Check that required files exist."""
    if not os.path.isfile(sim_exe):
        print(f"ERROR: Simulator not found: {sim_exe}")
        return False
    
    core = 0
    while core < 4:
        trace_path = f"{trace_prefix}_proc{core}.trace"
        if not os.path.isfile(trace_path):
            print(f"ERROR: Trace file missing: {trace_path}")
            return False
        core += 1
    
    return True


def run_experiments():
    """Main experiment runner."""
    
    # ========== CONFIGURATION ==========
    SIMULATOR = "./L1simulate"
    TRACE_NAME = "traces/app1"
    OUTPUT_CSV = "experiment_results.csv"
    GRAPH_FOLDER = "plots"
    
    # Experiment parameters
    BASE_S = 5
    BASE_E = 4
    BASE_B = 4
    
    FIXED_CACHE_BYTES = 2048  # 2KB constant cache
    
    # ===================================
    
    print("=" * 50)
    print("  CACHE PERFORMANCE ANALYSIS")
    print("=" * 50)
    
    if not validate_setup(SIMULATOR, TRACE_NAME):
        sys.exit(1)
    
    exp = CacheExperiment(SIMULATOR, TRACE_NAME)
    
    # ----- Experiment 1: Vary set bits (s) -----
    print("\n[Experiment 1] Varying set index bits...")
    s_val = 2
    while s_val <= 10:
        res = exp.execute_sim(s_val, BASE_E, BASE_B, "vary_s")
        exp.add_result(res)
        s_val += 1
    
    # ----- Experiment 2: Vary associativity (E) -----
    print("\n[Experiment 2] Varying associativity...")
    e_list = [1, 2, 4, 8, 16, 32, 64]
    e_idx = 0
    while e_idx < len(e_list):
        res = exp.execute_sim(BASE_S, e_list[e_idx], BASE_B, "vary_E")
        exp.add_result(res)
        e_idx += 1
    
    # ----- Experiment 3: Vary block size (b) -----
    print("\n[Experiment 3] Varying block bits...")
    b_val = 3
    while b_val <= 8:
        res = exp.execute_sim(BASE_S, BASE_E, b_val, "vary_b")
        exp.add_result(res)
        b_val += 1
    
    # ----- Experiment 4: Constant cache, vary s and b -----
    print(f"\n[Experiment 4] Constant cache ({FIXED_CACHE_BYTES}B), varying s and b...")
    const_sb_configs = [
        (2, 4, 7),  # 4 sets, 4 ways, 128B blocks = 2048
        (3, 4, 6),  # 8 sets, 4 ways, 64B blocks = 2048
        (4, 4, 5),  # 16 sets, 4 ways, 32B blocks = 2048
        (5, 4, 4),  # 32 sets, 4 ways, 16B blocks = 2048
        (6, 4, 3),  # 64 sets, 4 ways, 8B blocks = 2048
        (7, 4, 2),  # 128 sets, 4 ways, 4B blocks = 2048
    ]
    cfg_idx = 0
    while cfg_idx < len(const_sb_configs):
        s, e, b = const_sb_configs[cfg_idx]
        res = exp.execute_sim(s, e, b, "const_cache_sb")
        exp.add_result(res)
        cfg_idx += 1
    
    # ----- Experiment 5: Constant cache, vary E and b -----
    print(f"\n[Experiment 5] Constant cache ({FIXED_CACHE_BYTES}B), varying E and b...")
    const_eb_configs = [
        (5, 2, 5),   # 32 sets, 2 ways, 32B blocks = 2048
        (5, 4, 4),   # 32 sets, 4 ways, 16B blocks = 2048
        (5, 8, 3),   # 32 sets, 8 ways, 8B blocks = 2048
        (5, 16, 2),  # 32 sets, 16 ways, 4B blocks = 2048
    ]
    cfg_idx = 0
    while cfg_idx < len(const_eb_configs):
        s, e, b = const_eb_configs[cfg_idx]
        res = exp.execute_sim(s, e, b, "const_cache_eb")
        exp.add_result(res)
        cfg_idx += 1
    
    # ----- Experiment 6: Constant cache, vary s and E -----
    print(f"\n[Experiment 6] Constant cache ({FIXED_CACHE_BYTES}B), varying s and E...")
    const_se_configs = [
        (3, 32, 3),  # 8 sets, 32 ways, 8B blocks = 2048
        (4, 16, 3),  # 16 sets, 16 ways, 8B blocks = 2048
        (5, 8, 3),   # 32 sets, 8 ways, 8B blocks = 2048
        (6, 4, 3),   # 64 sets, 4 ways, 8B blocks = 2048
        (7, 2, 3),   # 128 sets, 2 ways, 8B blocks = 2048
    ]
    cfg_idx = 0
    while cfg_idx < len(const_se_configs):
        s, e, b = const_se_configs[cfg_idx]
        res = exp.execute_sim(s, e, b, "const_cache_se")
        exp.add_result(res)
        cfg_idx += 1
    
    # Export data
    print("\n" + "-" * 50)
    exp.export_csv(OUTPUT_CSV)
    
    # Generate plots
    if PLOTTING_AVAILABLE and len(exp.collected_data) > 0:
        print("\nGenerating graphs...")
        df = exp.get_dataframe()
        plotter = GraphGenerator(GRAPH_FOLDER)
        
        # Individual parameter plots
        plotter.create_line_chart(
            df, 's', 'exec_cycles',
            'Execution Time vs Set Index Bits',
            'Set Index Bits (s)', 'Execution Cycles',
            'exp1_set_bits.png', 'vary_s'
        )
        
        plotter.create_line_chart(
            df, 'E', 'exec_cycles',
            'Execution Time vs Associativity',
            'Associativity (E)', 'Execution Cycles',
            'exp2_associativity.png', 'vary_E'
        )
        
        plotter.create_line_chart(
            df, 'b', 'exec_cycles',
            'Execution Time vs Block Bits',
            'Block Bits (b)', 'Execution Cycles',
            'exp3_block_bits.png', 'vary_b'
        )
        
        # Constant cache plots
        plotter.create_line_chart(
            df, 's', 'exec_cycles',
            f'Execution Time vs Set Bits (Fixed {FIXED_CACHE_BYTES}B Cache)',
            'Set Index Bits (s)', 'Execution Cycles',
            'exp4_const_cache_s.png', 'const_cache_sb'
        )
        
        plotter.create_line_chart(
            df, 'E', 'exec_cycles',
            f'Execution Time vs Associativity (Fixed {FIXED_CACHE_BYTES}B Cache)',
            'Associativity (E)', 'Execution Cycles',
            'exp5_const_cache_E.png', 'const_cache_eb'
        )
        
        plotter.create_line_chart(
            df, 's', 'exec_cycles',
            f'Execution Time vs Set Bits (Fixed {FIXED_CACHE_BYTES}B Cache, b=3)',
            'Set Index Bits (s)', 'Execution Cycles',
            'exp6_const_cache_se.png', 'const_cache_se'
        )
        
        # Comparison plots
        plotter.create_comparison_chart(
            df, ['vary_s', 'vary_E', 'vary_b'],
            'cache_size', 'exec_cycles',
            'Execution Time vs Cache Size (Different Parameters)',
            'Cache Size (bytes)', 'Execution Cycles',
            'comparison_cache_size.png'
        )
        
        plotter.create_comparison_chart(
            df, ['const_cache_sb', 'const_cache_eb', 'const_cache_se'],
            's', 'exec_cycles',
            f'Execution Time Comparison (Fixed {FIXED_CACHE_BYTES}B Cache)',
            'Set Index Bits (s)', 'Execution Cycles',
            'comparison_const_cache.png'
        )
    
    print("\n" + "=" * 50)
    print("  ANALYSIS COMPLETE")
    print("=" * 50)


if __name__ == "__main__":
    run_experiments()
