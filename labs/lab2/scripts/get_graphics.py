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
        'size': [], 'threads': [], 'threshold': [],
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
                
            # ОБНОВЛЕННЫЙ РЕГУЛЯРНЫЙ ВЫРАЖЕНИЕ: убрал "Глубина", добавил "Потоки"
            match = re.search(r'Размер:\s+(\d+).*?Потоки:\s+(\d+).*?Порог пар\.:\s+(\d+).*?Послед\.:\s+([\d.]+).*?Паралл\.:\s+([\d.]+).*?Ускорение:\s+([\d.]+)', line)
            if match:
                data['size'].append(int(match.group(1)))
                data['threads'].append(int(match.group(2)))  # Теперь threads вместо depth
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
        key = (data['size'][i], data['threads'][i], data['threshold'][i])
        groups[key].append(i)
    
    avg_data = {
        'size': [], 'threads': [], 'threshold': [],
        'sequential': [], 'parallel': [], 'speedup': []
    }
    
    for key, indices in groups.items():
        avg_data['size'].append(key[0])
        avg_data['threads'].append(key[1])
        avg_data['threshold'].append(key[2])
        avg_data['sequential'].append(np.mean([data['sequential'][i] for i in indices]))
        avg_data['parallel'].append(np.mean([data['parallel'][i] for i in indices]))
        avg_data['speedup'].append(np.mean([data['speedup'][i] for i in indices]))
    
    return avg_data

def plot_size_impact(data, output_dir, output_prefix=""):
    # ОБНОВЛЕНО: фильтруем по threads=8 вместо depth=8
    filtered = {}
    for i in range(len(data['size'])):
        if data['threads'][i] == 8 and data['threshold'][i] == 1000:
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

def plot_threads_impact(data, output_dir, output_prefix=""):
    # НОВАЯ ФУНКЦИЯ: влияние количества потоков вместо глубины
    filtered = {}
    for i in range(len(data['threads'])):
        if data['size'][i] == 50000000 and data['threshold'][i] == 1000:
            threads = data['threads'][i]
            filtered[threads] = {
                'sequential': data['sequential'][i],
                'parallel': data['parallel'][i],
                'speedup': data['speedup'][i]
            }
    
    if not filtered:
        print("Нет данных для построения графиков влияния количества потоков")
        return None
    
    threads_list = sorted(filtered.keys())
    sequential = [filtered[t]['sequential'] for t in threads_list]
    parallel = [filtered[t]['parallel'] for t in threads_list]
    speedup = [filtered[t]['speedup'] for t in threads_list]
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    ax1.plot(threads_list, sequential, 'o-', label='Последовательная', linewidth=2)
    ax1.plot(threads_list, parallel, 'o-', label='Параллельная', linewidth=2)
    ax1.set_xlabel('Количество потоков')
    ax1.set_ylabel('Время (секунды)')
    ax1.set_title('Время выполнения vs Количество потоков')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    ax2.plot(threads_list, speedup, 'o-r', linewidth=2)
    ax2.set_xlabel('Количество потоков')
    ax2.set_ylabel('Ускорение')
    ax2.set_title('Ускорение vs Количество потоков')
    ax2.grid(True, alpha=0.3)
    
    # Добавим вертикальную линию на 12 потоков (hardware threads)
    ax2.axvline(x=12, color='gray', linestyle='--', alpha=0.7, label='12 HW threads')
    ax2.legend()
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, f'{output_prefix}threads_impact.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close(fig)
    
    return output_path

def plot_threshold_impact(data, output_dir, output_prefix=""):
    # ОБНОВЛЕНО: фильтруем по threads=8 вместо depth=4
    filtered = {}
    for i in range(len(data['threshold'])):
        if data['size'][i] == 50000000 and data['threads'][i] == 8:
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
        threads_path = plot_threads_impact(data, output_dir, file_prefix)  # ИЗМЕНИЛ НАЗВАНИЕ
        threshold_path = plot_threshold_impact(data, output_dir, file_prefix)
        
        print("Графики созданы:")
        if size_path: print(f"  {size_path}")
        if threads_path: print(f"  {threads_path}")
        if threshold_path: print(f"  {threshold_path}")
        
    except FileNotFoundError:
        print(f"Файл {filename} не найден")
    except Exception as e:
        print(f"Ошибка: {e}")