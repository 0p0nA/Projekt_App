Aplikacjia do monitorowania jakości powietrza.
----------------------------------------------

Opis:
Aplikacjia służy do pobierania, przetwarzania i wizualizacji danych dotyczących jakości powietrza.
Użytkownik za pomocą aplikacji może:
-wyświetlić dane dla stacji pomiarowych w Polsce
-wyświetlić dane dla stanowisk pomiarowych dla konretnej stacji
-wyświetlić dane pomiarowe mierzone dla danego stanowiska jednostka - [mg/m3]
-wyświetlić wykres danych pomiarowych dla dostępnego okresu czasu.

Dane za pomocą usługi REST pobierane są z API GIOŚ (Główny Inspektorat Ochrony Środowiska).
Aplikacja obsługuje brak połączenia z internetem przez wczytywanie informacji z lokalnej bazy danych (JSON).
----------------------------------------------

Wymagania:
-System operacyjny Windows
-Biblioteki:
    -wxWidgets
    -libcurl
    -nlohmann/json

----------------------------------------------
Autor: Adrian Gaca
