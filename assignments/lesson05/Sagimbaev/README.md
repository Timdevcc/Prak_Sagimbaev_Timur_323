# ДЗ 5 — Сагимбаев Тимур, 323

Сценарий: короткий чат с памятью. C++-клиент хранит историю, отправляет её локальному llama-server по HTTP и проверяет ответы. Интернет нужен для установки, затем программа работает локально.

## Окружение и модель

- Windows 11 Home Single Language, AMD Ryzen 5 7535HS, 16 ГБ RAM (15,2 ГиБ доступно ОС); CPU-режим, GPU не используется.
- Модель: [Qwen2.5-0.5B-Instruct-GGUF](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF).
- Файл: `qwen2.5-0.5b-instruct-q4_k_m.gguf`, квантование Q4_K_M, примерно 491 МБ.
- Контекст: 1024 токена, ответ: максимум 64 токена, температура 0.2.
- Сервер: CPU Windows x64, версия `0.7.0-dev`, build 11788, commit `74b115ca6`, Clang 20.1.8. Модель и сервер находятся вне Git, в `D:\llama-lesson05`.

## Установка на Windows

Нужны Visual Studio с компонентами разработки C++ и CMake 3.20+. Команды ниже выполнять из этого каталога в Developer PowerShell for VS:

```powershell
cmake -S . -B build-native -A x64
cmake --build build-native --config Debug
ctest --test-dir build-native -C Debug --output-on-failure
```

libcurl 8.12.1 и nlohmann JSON v3.11.3 скачиваются через CMake FetchContent. При первой сборке нужен интернет. Собирается статический libcurl только для HTTP, без внешних зависимостей и TLS: это подходит для локального сервера. Исполняемый файл находится в `build-native/Debug`. Для своей установленной libcurl (например, с HTTPS) передайте `-DCHAT_FETCH_CURL=OFF` и при необходимости toolchain vcpkg.

На Linux с установленными CMake, компилятором и пакетом разработки libcurl:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCHAT_FETCH_CURL=OFF
cmake --build build
ctest --test-dir build --output-on-failure
./build/local_chat config.example.json
```

## Сервер

Скачайте [CPU-архив b11788](https://github.com/ggml-org/llama.cpp/releases/download/b11788/llama-b11788-bin-win-cpu-x64.zip), распакуйте вместе с DLL вне репозитория. Скачайте только один GGUF-файл:

```powershell
curl.exe -L --fail https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/qwen2.5-0.5b-instruct-q4_k_m.gguf -o D:/llama-lesson05/models/qwen2.5-0.5b-instruct-q4_k_m.gguf
```

Запуск (путь к exe может зависеть от структуры распакованного архива):

```powershell
D:/llama-lesson05/server/llama-server.exe -m D:/llama-lesson05/models/qwen2.5-0.5b-instruct-q4_k_m.gguf --alias local-model -c 1024 -ngl 0 -t 2 -np 1 --host 127.0.0.1 --port 8080
```

Оставьте сервер работающим. Откройте `http://127.0.0.1:8080` для веб-чата. В другом терминале:

```powershell
curl.exe --fail http://127.0.0.1:8080/health
curl.exe --fail-with-body --max-time 180 http://127.0.0.1:8080/v1/chat/completions -H "Content-Type: application/json" --data-binary "@examples/request.json"
.\build-native\Debug\local_chat.exe config.example.json
```

Ввод и файлы — UTF-8. `--help` выводит справку. Команда `/reset` очищает историю с сохранением system, `/exit` завершает ввод.

## Устройство

- `include/chat.hpp`: интерфейсы и структуры Message, Config, Reply.
- `src/chat.cpp`: проверка конфигурации, JSON-запроса, текста ответа, статистики и удаление старой пары.
- `src/http.cpp`: HTTP через libcurl, RAII, тайм-аут соединения 5 секунд, общий тайм-аут из конфигурации (180 секунд).
- `src/main.cpp`: CLI и управление историей `std::vector<Message>`.
- `tests/chat_tests.cpp`: автономные тесты конфигурации, экранирования, обрезки истории, структуры ответа и статистики.
- Цель `chat_logic` используется клиентом `local_chat` и тестом `chat_tests`.

Поле `messages` — массив реплик; `role` задаёт автора (`system`, `user`, `assistant`), `content` содержит текст. JSON создаётся библиотекой, кавычки и переносы экранируются автоматически.

Перед генерацией тот же запрос отправляется в `/v1/chat/completions/input_tokens`. Пока вход плюс 64 токена больше 1024, удаляется самая старая завершённая пара и вход пересчитывается. Системное сообщение и текущая реплика сохраняются. Если места всё равно нет, генерация не отправляется. При ошибке подсчёта генерации также нет.

Изменения истории фиксируются только после полного успешного ответа, включая проверку `usage`; при ошибке история остаётся прежней. После успеха выводятся токены входа, ответа, сумма, число удалённых пар и время всего хода через `steady_clock`. Отсутствующая статистика считается ошибкой. За сессию с любой ошибкой клиент возвращает код 1, даже если пользователь затем продолжил работу.

## Проверка на разборе

1. Запустить сервер, проверить веб-чат и HTTP-запрос.
2. Собрать клиент и запустить CTest.
3. Написать `My name is Timur.`, затем `What is my name?` — проверить память.
4. Продолжить диалог: показать рост prompt_tokens и удаление старых пар.
5. Ввести сообщение длиннее контекста: клиент должен предложить его сократить.
6. Остановить сервер и ввести реплику: понятная ошибка, ненулевой код завершения.

## Фактические результаты, 21 октября 2026

Сборка Debug в Visual Studio 2026 успешна. CTest: 1/1 тест пройден (проверки внутри `chat_tests`). Ответ сервера `/health`: `{"status":"ok"}`.

| Проверка | Фактический результат | Оценка |
| --- | --- | --- |
| Память: `My name is Timur. Remember it.` → `What is my name?` | `Your name is Timur.`; вход вырос с 32 до 58 токенов | Имя сохранено в истории и восстановлено правильно |
| 12 последовательных длинных реплик | Вход: 171 → 330 → 489 → 648 → 807; начиная с шестого хода удаляется одна старая пара | Контекст контролируется; все ответы `OK`, переполнения нет |
| Пограничный ввод: `word ` 2000 раз | `Message exceeds context budget; shorten your input. Generation was not sent.`; следующий короткий запрос получает `OK.` | Длинный ввод отклонён; история не испорчена, код сессии 1 |
| `/reset`, затем вопрос об имени | `My name is a digital assistant created by Alibaba Cloud.` | Имя Timur больше не используется; ответ на вопрос неверен, модель перепутала автора — ограничение качества 0.5B |
| Сервер выключен | Сообщение «Сервер недоступен…», код 1 | Ошибка соединения обработана |

Полные протоколы: [memory.txt](examples/memory.txt), [context.txt](examples/context.txt), [boundary.txt](examples/boundary.txt), [reset.txt](examples/reset.txt), [server-unavailable.txt](examples/server-unavailable.txt). Прямой HTTP-ответ сохранён в [response.json](examples/response.json), запрос — [request.json](examples/request.json).

Веб-интерфейс также проверен: на `Say hello in one short sentence.` модель ответила `Hello! How can I assist you today?`. [Скриншот](examples/web-chat.png).

Первый прямой HTTP-запрос: 36 входных и 11 выходных токенов. Полный ход в CLI с двумя HTTP-вызовами занял примерно 0,7–1,5 с в коротком диалоге; при обрезке длинной истории — 15–20 с. Это измерения на этом ноутбуке, а не гарантированная скорость. `usage` относится к текущему запросу, а не сумме всей сессии.

Чтобы повторно записать реальные примеры, при запущенном сервере выполните (нужен Python 3):

```powershell
python tests/record_examples.py build-native/Debug/local_chat.exe
```

Скрипт перезаписывает протоколы в `examples/`; тесты CTest не требуют сервера, модели или Python.

