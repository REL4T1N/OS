#include <math.h>
#include <stdio.h>

float sinIntegral(float A, float B, float e) {
    // Проверка корректности входных данных
    if (B <= A || e <= 0.0f) {
        fprintf(stderr, "Warning: Invalid input for sinIntegral: A=%.2f, B=%.2f, e=%.2f\n", A, B, e);
        return 0.0f;
    }
    
    float result = 0.0f;
    float current = A;
    
    // Метод трапеций
    while (current < B) {
        float next = (current + e < B) ? (current + e) : B;
        float height_left = sinf(current);
        float height_right = sinf(next);
        float average_height = (height_left + height_right) / 2.0f;
        float width = next - current;
        result += average_height * width;
        current = next;
    }
    
    return result;
}