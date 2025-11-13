#!/usr/bin/env python3
import matplotlib.pyplot as plt
import re
import sys
import numpy as np
import os

# Используем Agg backend для работы без GUI
plt.switch_backend('Agg')

def parse_logs(filename):
    data = {
        'size': [], 'threads': [], 'threshold': [],
        'sequential': [], 'parallel': [], 'speedup': []
    }
    
    with open(filename, 'r', encoding='utf-8') as f:
        for line in f:
            # Ищем строки с результатами тестов
            match = re.search(r'Размер:\s+(\d+).*?Потоки:\s+(\d+).*?Порог пар\.:\s+(\d+).*?Послед\.:\s+([\d.]+).*?Паралл\.:\s+([\d.]+).*?Ускорение:\s+([\d.]+)', line)
            if match:
                data['size'].append(int(match.group(1)))
                data['threads'].append(int(match.group(2)))
                data['threshold'].append(int(match.group(3)))
                data['sequential'].append(float(match.group(4)))
                data['parallel'].append(float(match.group(5)))
                data['speedup'].append(float(match.group(6)))
    
    return data

def plot_speedup_and_efficiency(data, output_dir):
    # Фильтруем данные для теста влияния потоков (размер=50000000, порог=1000)
    threads_data = {}
    for i in range(len(data['threads'])):
        if data['size'][i] == 50000000 and data['threshold'][i] == 1000:
            threads = data['threads'][i]
            threads_data[threads] = {
                'speedup': data['speedup'][i],
                'efficiency': data['speedup'][i] / threads  # Эффективность = ускорение / потоки
            }
    
    if not threads_data:
        print("Нет данных для построения графиков влияния количества потоков")
        return None
    
    threads_list = sorted(threads_data.keys())
    speedup = [threads_data[t]['speedup'] for t in threads_list]
    efficiency = [threads_data[t]['efficiency'] for t in threads_list]
    
    # Создаем два графика
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # График 1: Ускорение
    ax1.plot(threads_list, speedup, 'o-', color='blue', linewidth=3, markersize=8, label='Ускорение')
    ax1.set_xlabel('Количество потоков', fontsize=12)
    ax1.set_ylabel('Ускорение', fontsize=12)
    ax1.set_title('Зависимость ускорения от количества потоков', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend(fontsize=11)
    ax1.set_xticks(threads_list)
    
    # График 2: Эффективность
    ax2.plot(threads_list, efficiency, 's-', color='green', linewidth=3, markersize=8, label='Эффективность')
    ax2.set_xlabel('Количество потоков', fontsize=12)
    ax2.set_ylabel('Эффективность', fontsize=12)
    ax2.set_title('Зависимость эффективности от количества потоков', fontsize=14, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=11)
    ax2.set_xticks(threads_list)
    
    # Устанавливаем пределы и шаг для оси Y эффективности
    ax2.set_ylim(0, 1.0)  # от 0 до 1
    ax2.set_yticks(np.arange(0, 1.1, 0.1))  # шаг 0.1 (0, 0.1, 0.2, ..., 1.0)
    
    plt.tight_layout()
    
    # Сохраняем оба графика в один файл
    output_path = os.path.join(output_dir, 'speedup_efficiency.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close(fig)
    
    print(f"Графики сохранены: {output_path}")
        
    return output_path

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Использование: python3 graphics.py <файл_логов> [выходная_директория]")
        sys.exit(1)
    
    filename = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "."
    
    os.makedirs(output_dir, exist_ok=True)
    
    try:
        data = parse_logs(filename)
        plot_speedup_and_efficiency(data, output_dir)
        
    except FileNotFoundError:
        print(f"Файл {filename} не найден")
    except Exception as e:
        print(f"Ошибка: {e}")