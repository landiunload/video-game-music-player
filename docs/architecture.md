# Архитектура

Приложение намеренно не тянет весь игровой движок. Из наработок `laiue`
оставлена узкая платформа, необходимая музыкальному плееру.

```text
Win32 window/input/time
        │
        ├── D3D12 UI renderer ── immediate UI + GDI font atlas + WIC cover
        │
        ├── Media Foundation audio player
        │
        └── radio source worker ── WinHTTP ── Khinsider parser
                                      │
                                      └── local cache ── audio player
```

Сеть никогда не выполняется в кадре интерфейса. Рабочий поток получает
страницу случайного трека, проверяет финальный HTTPS-origin и аудиохост,
разбирает метаданные и атомарно дописывает файлы в кэш. Главный поток видит
только готовый `Track` либо сообщение об ошибке.

`AudioPlayer` владеет Media Engine и отдаёт главному потоку неблокирующую
очередь событий. Отрисовка использует D3D12 swap chain, два кадра в полёте,
upload ring и vertex pulling для списка UI-квадов. Отдельного игрового
рендерера, сцены или asset pipeline в проекте нет.

Состояние хранится отдельно от кэша в
`%LOCALAPPDATA%\laiue-radio\state.bin`. Перед заменой файл полностью
записывается во временный `state.bin.part`.

