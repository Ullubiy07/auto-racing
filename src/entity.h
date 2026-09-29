#ifndef ENTITY_H
#define ENTITY_H

#include "car.h"

typedef enum {
	empty,   // Пустая клетка
	locked,  // Недоступная клетка
	road,    // Клетка дороги
	car,     // Машина
} EntityType;

// Обобщенная структура, описывающая сущность гонки
typedef struct {
	EntityType type;  // Тип сущности
	int y;            // Позиция по вертикали 
	int x;            // Позиция по горизонтали
	Car car;          // Если является автомобилем
} Entity;

// Получение типа сущности по символу
EntityType GetEntityType(char c);

#endif
