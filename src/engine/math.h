#pragma once

// Скалярные помощники ядра. Плееру из всей математики движка нужен
// только зажим диапазона (масштаб UI, прогресс, анимации наведения).

float ScalarClamp(float value, float minimum, float maximum);
