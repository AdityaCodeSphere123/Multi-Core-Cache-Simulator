import subprocess
import re
import pandas as pd
import matplotlib.pyplot as plt
import os

# Configuration
EXECUTABLE = "./cache_simulator.exe"  # Path to the simulator executable
TRACE_PREFIX = "input"               # Trace file prefix (e.g., input_proc0.trace)
RESULTS_FILE = "cache_sim1_results.csv"
PLOT_DIR = "plots"                   # Directory to save plots
CONSTANT_CACHE_SIZE = 4096           # Fixed cache size in bytes (2^6 * 2 * 2^5 = 4096)

# Parameter values for independent variations
S_VALUES = list(range(1, 12))        # Set index bits: 1 to 11
E_VALUES = list(range(1, 101))       # Associativity: 1 to 100
B_VALUES = list(range(1, 16))        # Block bits: 1 to 15

# Default parameters
DEFAULT_S = 6
DEFAULT_E = 2
DEFAULT_B = 5

# Configurations for constant cache size
# s and b (fix E=2): s=6,b=5; s=7,b=4; s=8,b=3; s=9,b=2
SB_CONST_CONFIGS = [
    {"s": 1, "E": 2, "b": 10},
    {"s": 2, "E": 2, "b": 9},
    {"s": 3, "E": 2, "b": 8},
    {"s": 4, "E": 2, "b": 7},
    {"s": 5, "E": 2, "b": 6},
    {"s": 6, "E": 2, "b": 5},
    {"s": 7, "E": 2, "b": 4},
    {"s": 8, "E": 2, "b": 3},
    {"s": 9, "E": 2, "b": 2},
    {"s": 10, "E": 2, "b": 1}
]

# b and E (fix s=6): b=5,E=2; b=4,E=4; b=3,E=8; b=2,E=16
BE_CONST_CONFIGS = [
    {"s": 6, "E": 1, "b": 6},
    {"s": 6, "E": 2, "b": 5},
    {"s": 6, "E": 4, "b": 4},
    {"s": 6, "E": 8, "b": 3},
    {"s": 6, "E": 16, "b": 2},
    {"s": 6, "E": 32, "b": 1}
]

# s and E (fix b=5): s=3,E=16; s=4,E=8; s=5,E=4; s=6,E=2
SE_CONST_CONFIGS = [
    {"s": 1, "E": 64, "b": 5},
    {"s": 2, "E": 32, "b": 5},
    {"s": 3, "E": 16, "b": 5},
    {"s": 4, "E": 8, "b": 5},
    {"s": 5, "E": 4, "b": 5},
    {"s": 6, "E": 2, "b": 5},
    {"s": 7, "E": 1, "b": 5}
]

# Ensure plot directory exists
if not os.path.exists(PLOT_DIR):
    os.makedirs(PLOT_DIR)

# Function to run the simulator and extract max execution time
def run_simulation(param, value, s, E, b):
    cache_size = (1 << s) * E * (1 << b)  # Cache size in bytes
    cmd = [EXECUTABLE, "-t", TRACE_PREFIX, "-s", str(s), "-E", str(E), "-b", str(b)]
    
    print(f"Running simulation with {param}={value} (s={s}, E={E}, b={b}, CacheSize={cache_size} bytes)...")
    
    try:
        # Run the simulator and capture output
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        output = result.stdout
        
        # Extract max execution time (last occurrence of the pattern)
        match = re.search(r"Maximum Execution Time \(cycles\): (\d+)", output)
        if not match:
            print(f"Error: Could not extract MaxExecutionTime for {param}={value}.")
            return None
        max_time = int(match.group(1))
        
        return {"Parameter": param, "Value": value, "MaxExecutionTime": max_time, "CacheSize": cache_size, "s": s, "E": E, "b": b}
    
    except subprocess.CalledProcessError as e:
        print(f"Error: Simulation failed for {param}={value}. Error: {e}")
        return None
    except Exception as e:
        print(f"Error: Unexpected error for {param}={value}. Error: {e}")
        return None

# Check if trace files exist
for i in range(4):
    trace_file = f"{TRACE_PREFIX}_proc{i}.trace"
    if not os.path.exists(trace_file):
        print(f"Error: Trace file {trace_file} not found.")
        exit(1)

# Check if executable exists
if not os.path.exists(EXECUTABLE):
    print(f"Error: Simulator executable {EXECUTABLE} not found.")
    exit(1)

# Collect results
results = []

# Vary set index bits (s) independently
for s in S_VALUES:
    result = run_simulation("SetIndexBits", s, s, DEFAULT_E, DEFAULT_B)
    if result:
        results.append(result)

# Vary associativity (E) independently
for E in E_VALUES:
    result = run_simulation("Associativity", E, DEFAULT_S, E, DEFAULT_B)
    if result:
        results.append(result)

# Vary block bits (b) independently
for b in B_VALUES:
    result = run_simulation("BlockBits", b, DEFAULT_S, DEFAULT_E, b)
    if result:
        results.append(result)

# Constant cache size: s and b (fix E=2)
for config in SB_CONST_CONFIGS:
    s, E, b = config["s"], config["E"], config["b"]
    result = run_simulation(f"SetIndexBits_ConstCache_{CONSTANT_CACHE_SIZE}", s, s, E, b)
    if result:
        results.append(result)

# Constant cache size: b and E (fix s=6)
for config in BE_CONST_CONFIGS:
    s, E, b = config["s"], config["E"], config["b"]
    result = run_simulation(f"BlockBits_ConstCache_{CONSTANT_CACHE_SIZE}", b, s, E, b)
    if result:
        results.append(result)

# Constant cache size: s and E (fix b=5)
for config in SE_CONST_CONFIGS:
    s, E, b = config["s"], config["E"], config["b"]
    result = run_simulation(f"SetIndexBits_ConstCache_{CONSTANT_CACHE_SIZE}_E", s, s, E, b)
    if result:
        results.append(result)

# Save results to CSV
if results:
    df = pd.DataFrame(results)
    df.to_csv(RESULTS_FILE, index=False)
    print(f"Results saved to {RESULTS_FILE}")
else:
    print("No results to save. Exiting.")
    exit(1)

# Plotting
# Split data by parameter
set_index_data = df[df['Parameter'] == 'SetIndexBits']
associativity_data = df[df['Parameter'] == 'Associativity']
block_bits_data = df[df['Parameter'] == 'BlockBits']
set_index_const_data = df[df['Parameter'] == f"SetIndexBits_ConstCache_{CONSTANT_CACHE_SIZE}"]
block_bits_const_data = df[df['Parameter'] == f"BlockBits_ConstCache_{CONSTANT_CACHE_SIZE}"]
set_index_const_e_data = df[df['Parameter'] == f"SetIndexBits_ConstCache_{CONSTANT_CACHE_SIZE}_E"]

# Plot 1: Maximum Execution Time vs. Set Index Bits (Independent)
plt.figure(figsize=(8, 6))
plt.plot(set_index_data['Value'], set_index_data['MaxExecutionTime'], marker='o', label='Set Index Bits')
plt.xlabel('Set Index Bits (s)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title('Maximum Execution Time vs. Set Index Bits')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, 'set_index_bits2.png'))
plt.close()

# Plot 2: Maximum Execution Time vs. Associativity (Independent)
plt.figure(figsize=(8, 6))
plt.plot(associativity_data['Value'], associativity_data['MaxExecutionTime'], marker='s', label='Associativity')
plt.xlabel('Associativity (E)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title('Maximum Execution Time vs. Associativity')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, 'associativity2.png'))
plt.close()

# Plot 3: Maximum Execution Time vs. Block Bits (Independent)
plt.figure(figsize=(8, 6))
plt.plot(block_bits_data['Value'], block_bits_data['MaxExecutionTime'], marker='^', label='Block Bits')
plt.xlabel('Block Bits (b)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title('Maximum Execution Time vs. Block Bits')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, 'block_bits2.png'))
plt.close()

# Plot 4: Maximum Execution Time vs. Cache Size (Independent Variations)
plt.figure(figsize=(8, 6))
plt.plot(set_index_data['CacheSize'], set_index_data['MaxExecutionTime'], marker='o', label='Varying s')
plt.plot(associativity_data['CacheSize'], associativity_data['MaxExecutionTime'], marker='s', label='Varying E')
plt.plot(block_bits_data['CacheSize'], block_bits_data['MaxExecutionTime'], marker='^', label='Varying b')
plt.xlabel('Cache Size (bytes)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title('Maximum Execution Time vs. Cache Size')
plt.legend()
plt.grid(True)
plt.savefig(os.path.join(PLOT_DIR, 'cache_size2.png'))
plt.close()

# Plot 5: Maximum Execution Time vs. Set Index Bits (Constant Cache Size, Varying s and b, E=2)
plt.figure(figsize=(8, 6))
plt.plot(set_index_const_data['Value'], set_index_const_data['MaxExecutionTime'], marker='o', linestyle='-', label='E=2')
plt.xlabel('Set Index Bits (s)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title(f'Maximum Execution Time vs. Set Index Bits (Constant Cache Size = {CONSTANT_CACHE_SIZE} bytes, E=2)')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, f'set_index_bits_const_{CONSTANT_CACHE_SIZE}.png'))
plt.close()

# Plot 6: Maximum Execution Time vs. Block Bits (Constant Cache Size, Varying b and E, s=6)
plt.figure(figsize=(8, 6))
plt.plot(block_bits_const_data['Value'], block_bits_const_data['MaxExecutionTime'], marker='^', linestyle='-', label='s=6')
plt.xlabel('Block Bits (b)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title(f'Maximum Execution Time vs. Block Bits (Constant Cache Size = {CONSTANT_CACHE_SIZE} bytes, s=6)')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, f'block_bits_const_{CONSTANT_CACHE_SIZE}.png'))
plt.close()

# Plot 7: Maximum Execution Time vs. Set Index Bits (Constant Cache Size, Varying s and E, b=5)
plt.figure(figsize=(8, 6))
plt.plot(set_index_const_e_data['Value'], set_index_const_e_data['MaxExecutionTime'], marker='o', linestyle='-', label='b=5')
plt.xlabel('Set Index Bits (s)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title(f'Maximum Execution Time vs. Set Index Bits (Constant Cache Size = {CONSTANT_CACHE_SIZE} bytes, b=5)')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, f'set_index_bits_const_e_{CONSTANT_CACHE_SIZE}.png'))
plt.close()

# Plot 8: Maximum Execution Time vs. Associativity (Constant Cache Size, Varying s and E, b=5)
plt.figure(figsize=(8, 6))
plt.plot(set_index_const_e_data['E'], set_index_const_e_data['MaxExecutionTime'], marker='s', linestyle='-', label='b=5')
plt.xlabel('Associativity (E)')
plt.ylabel('Maximum Execution Time (cycles)')
plt.title(f'Maximum Execution Time vs. Associativity (Constant Cache Size = {CONSTANT_CACHE_SIZE} bytes, b=5)')
plt.grid(True)
plt.legend()
plt.savefig(os.path.join(PLOT_DIR, f'associativity_const_e_{CONSTANT_CACHE_SIZE}.png'))
plt.close()

print(f"Plots saved in {PLOT_DIR}/: set_index_bits2.png, associativity2.png, block_bits2.png, cache_size2.png, "
      f"set_index_bits_const_{CONSTANT_CACHE_SIZE}.png, block_bits_const_{CONSTANT_CACHE_SIZE}.png, "
      f"set_index_bits_const_e_{CONSTANT_CACHE_SIZE}.png, associativity_const_e_{CONSTANT_CACHE_SIZE}.png")