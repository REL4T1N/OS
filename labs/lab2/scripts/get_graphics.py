#!/usr/bin/env python3
import matplotlib.pyplot as plt
import re
import sys
import numpy as np
import os
from collections import defaultdict

# Используем Agg backend для работы без GUI
plt.switch_backend('Agg')

def parse_logs(filename):
    data = {
        'size': [], 'depth': [], 'threshold': [],
        'sequential': [], 'parallel': [], 'speedup': [],
        'iteration': []
    }
    
    current_iteration = 1
    
    with open(filename, 'r', encoding='utf-8') as f:
        for line in f:
            if '=== Итерация' in line:
                match = re.search(r'Итерация (\d+)', line)
                if match:
                    current_iteration = int(match.group(1))
                continue
                
            match = re.search(r'Размер:\s+(\d+).*?Глубина:\s+(\d+).*?Порог пар\.:\s+(\d+).*?Послед\.:\s+([\d.]+).*?Паралл\.:\s+([\d.]+).*?Ускорение:\s+([\d.]+)', line)
            if match:
                data['size'].append(int(match.group(1)))
                data['depth'].append(int(match.group(2)))
                data['threshold'].append(int(match.group(3)))
                data['sequential'].append(float(match.group(4)))
                data['parallel'].append(float(match.group(5)))
                data['speedup'].append(float(match.group(6)))
                data['iteration'].append(current_iteration)
    
    return data

def calculate_averages(data):
    if max(data['iteration']) == 1:
        return data
    
    groups = defaultdict(list)
    for i in range(len(data['size'])):
        key = (data['size'][i], data['depth'][i], data['threshold'][i])
        groups[key].append(i)
    
    avg_data = {
        'size': [], 'depth': [], 'threshold': [],
        'sequential': [], 'parallel': [], 'speedup': []
    }
    
    for key, indices in groups.items():
        avg_data['size'].append(key[0])
        avg_data['depth'].append(key[1])
        avg_data['threshold'].append(key[2])
        avg_data['sequential'].append(np.mean([data['sequential'][i] for i in indices]))
        avg_data['parallel'].append(np.mean([data['parallel'][i] for i in indices]))
        avg_data['speedup'].append(np.mean([data['speedup'][i] for i in indices]))
    
    return avg_data

def plot_size_impact(data, output_dir, output_prefix=""):
    filtered = {}
    for i in range(len(data['size'])):
        if data['depth'][i] == 8 and data['threshold'][i] == 1000:
            size = data['size'][i]
            filtered[size] = {
                'sequential': data['sequential'][i],
                'parallel': data['parallel'][i],
                'speedup': data['speedup'][i]
            }
    
    if not filtered:
        print("Нет данных для построения графиков влияния размера")
        return None
    
    sizes = sorted(filtered.keys())
    sequential = [filtered[s]['sequential'] for s in sizes]
    parallel = [filtered[s]['parallel'] for s in sizes]
    speedup = [filtered[s]['speedup'] for s in sizes]
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    ax1.plot(sizes, sequential, 'o-', label='Последовательная', linewidth=2)
    ax1.plot(sizes, parallel, 'o-', label='Параллельная', linewidth=2)
    ax1.set_xlabel('Размер массива')
    ax1.set_ylabel('Время (секунды)')
    ax1.set_title('Время выполнения vs Размер массива')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    ax2.plot(sizes, speedup, 'o-r', linewidth=2)
    ax2.set_xlabel('Размер массива')
    ax2.set_ylabel('Ускорение')
    ax2.set_title('Ускорение vs Размер массива')
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, f'{output_prefix}size_impact.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close(fig)
    
    return output_path

def plot_depth_impact(data, output_dir, output_prefix=""):
    filtered = {}
    for i in range(len(data['depth'])):
        if data['size'][i] == 50000000 and data['threshold'][i] == 1000:
            depth = data['depth'][i]
            filtered[depth] = {
                'sequential': data['sequential'][i],
                'parallel': data['parallel'][i],
                'speedup': data['speedup'][i]
            }
    
    if not filtered:
        print("Нет данных для построения графиков влияния глубины")
        return None
    
    depths = sorted(filtered.keys())
    sequential = [filtered[d]['sequential'] for d in depths]
    parallel = [filtered[d]['parallel'] for d in depths]
    speedup = [filtered[d]['speedup'] for d in depths]
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    ax1.plot(depths, sequential, 'o-', label='Последовательная', linewidth=2)
    ax1.plot(depths, parallel, 'o-', label='Параллельная', linewidth=2)
    ax1.set_xlabel('Глубина параллелизма')
    ax1.set_ylabel('Время (секунды)')
    ax1.set_title('Время выполнения vs Глубина параллелизма')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    ax2.plot(depths, speedup, 'o-r', linewidth=2)
    ax2.set_xlabel('Глубина параллелизма')
    ax2.set_ylabel('Ускорение')
    ax2.set_title('Ускорение vs Глубина параллелизма')
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, f'{output_prefix}depth_impact.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close(fig)
    
    return output_path

def plot_threshold_impact(data, output_dir, output_prefix=""):
    filtered = {}
    for i in range(len(data['threshold'])):
        if data['size'][i] == 50000000 and data['depth'][i] == 4:
            threshold = data['threshold'][i]
            filtered[threshold] = {
                'sequential': data['sequential'][i],
                'parallel': data['parallel'][i],
                'speedup': data['speedup'][i]
            }
    
    if not filtered:
        print("Нет данных для построения графиков влияния порога")
        return None
    
    thresholds = sorted(filtered.keys())
    sequential = [filtered[t]['sequential'] for t in thresholds]
    parallel = [filtered[t]['parallel'] for t in thresholds]
    speedup = [filtered[t]['speedup'] for t in thresholds]
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    ax1.plot(thresholds, sequential, 'o-', label='Последовательная', linewidth=2)
    ax1.plot(thresholds, parallel, 'o-', label='Параллельная', linewidth=2)
    ax1.set_xlabel('Порог параллелизма')
    ax1.set_ylabel('Время (секунды)')
    ax1.set_title('Время выполнения vs Порог параллелизма')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    ax2.plot(thresholds, speedup, 'o-r', linewidth=2)
    ax2.set_xlabel('Порог параллелизма')
    ax2.set_ylabel('Ускорение')
    ax2.set_title('Ускорение vs Порог параллелизма')
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, f'{output_prefix}threshold_impact.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close(fig)
    
    return output_path

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Использование: python3 get_graphics.py <файл_логов> [префикс_выходных_файлов]")
        sys.exit(1)
    
    filename = sys.argv[1]
    output_prefix = sys.argv[2] if len(sys.argv) > 2 else ""
    
    if output_prefix:
        output_dir = os.path.dirname(output_prefix)
        file_prefix = os.path.basename(output_prefix)
    else:
        output_dir = "."
        file_prefix = ""
    
    os.makedirs(output_dir, exist_ok=True)
    
    try:
        data = parse_logs(filename)
        
        if 'benchmark' in filename.lower():
            data = calculate_averages(data)
            file_prefix = "benchmark_" if not file_prefix else file_prefix
        
        print(f"Построение графиков из: {filename}")
        
        size_path = plot_size_impact(data, output_dir, file_prefix)
        depth_path = plot_depth_impact(data, output_dir, file_prefix)
        threshold_path = plot_threshold_impact(data, output_dir, file_prefix)
        
        print("Графики созданы:")
        if size_path: print(f"  {size_path}")
        if depth_path: print(f"  {depth_path}")
        if threshold_path: print(f"  {threshold_path}")
        
    except FileNotFoundError:
        print(f"Файл {filename} не найден")
    except Exception as e:
        print(f"Ошибка: {e}")