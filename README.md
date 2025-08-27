# Сервис социальной сети (курс Highload Architect)

## ДЗ 9: Отказоустойчивость приложений

### Предварительная подготовка

* собрать новую версию сервиса **social_network**, для этого выполнить скрипт `build.social_network.sh` в корне репозитория.  
В результате, с помощью образа **alpine-cpp-builder:2** будет произведена компиляция бинарника и генерация Docker-файла в каталоге `service/_build`, а далее выполнится сборка образа сервиса **social_network:9**

* развернуть систему

```bash
docker compose -f docker-compose.hw-09.yml up -d

# по окончании работы остановить систему командой
docker compose -f docker-compose.hw-09.yml down --remove-orphans
```

В результате будет развернут ряд сервисов, среди них:

* `social_network:9`
* `haproxy:2.4.29-alpine`
* `nginx:1.28.0-alpine`

### Описание

### Выводы
