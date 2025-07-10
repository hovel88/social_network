#!/usr/bin/env python3

import csv
from faker import Faker

FILE_INTERESTS_RAW = "interests_raw.csv"
FILE_CITIES_RAW    = "cities_raw.csv"
FILE_PEOPLE_RAW    = "people_raw.csv"

def read_lines_from_file(filename):
    """
    Аргументы:
        filename: Имя файла, который нужно прочитать.

    Возвращает:
        Список строк из файла.
    """
    try:
        with open(filename, 'r', encoding='utf-8') as file:
            lines = file.readlines()
        return [item.strip() for item in lines] # подчищаем от лишних символов
    except FileNotFoundError:
        print(f"Ошибка: Файл '{filename}' не найден.")
        return None
    except Exception as e:
        print(f"Ошибка при чтении файла: {e}")
        return None

def split_people_names(people):
    """
    Аргументы:
        people: Список строк в формате "Фамилия Имя".

    Возвращает:
        Список строк кортежей <"Фамилия", "Имя">.
    """
    return [tuple(item.split()) for item in people]

# -----------------------------------------------------------------------------

interests = read_lines_from_file(FILE_INTERESTS_RAW)
cities    = read_lines_from_file(FILE_CITIES_RAW)
people    = split_people_names(read_lines_from_file(FILE_PEOPLE_RAW))

fake     = Faker('ru_RU')
# Пароль "secret123" в формате bcrypt для всех пользователей
pwd_hash = '$2a$12$XH4BS2zgGgpJs4hiu9p17OwxHxoWto21DLHkzo6JQH67U/3wi.LEW'

with open('users.csv', 'w', newline='') as csvfile:
    fieldnames = ['second_name', 'first_name', 'birthdate', 'biography', 'city', 'pwd_hash']
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

    writer.writeheader()

    interests_max_cnt = len(interests)-1
    cities_max_cnt    = len(cities)-1

    for item in people:
        rand_1 = fake.random_int(min=0, max=interests_max_cnt)
        rand_2 = fake.random_int(min=0, max=interests_max_cnt)
        rand_3 = fake.random_int(min=0, max=interests_max_cnt)
        interest = ', '.join([interests[rand_1], interests[rand_2], interests[rand_3]])

        writer.writerow({
            'second_name': item[0],
            'first_name': item[1],
            'birthdate': fake.date_of_birth(minimum_age=18, maximum_age=70).strftime('%Y-%m-%d'), # в формате 2017-02-01
            'biography': interest,
            'city': cities[fake.random_int(min=0, max=cities_max_cnt)],
            'pwd_hash': pwd_hash
        })
