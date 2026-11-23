# Pocket Arcade: що зробити Івану

Код, ігри й тести готові. Зіп для магазину: `02-Projects/bp26-badge-gameboy/files/pocket-arcade-submit-0.1.0.zip` у vault.

## 1. Опублікувати приховано і перевірити на бейджі (обовʼязково)

Перевірити в браузерному емуляторі я не зміг: автоматичний фільтр безпеки не дав вставити файли у сторінку емулятора. Інтерпретатор там і так дає менше 1 кадру/с, тому ігри перевіряються лише на бейджі.

1. Відкрий https://solana.com/breakpoint/badge/apps/submit (акаунт із привʼязаним X).
2. Перетягни зіп або теку `~/Projects/pocket-arcade/app`.
3. «Try in emulator»: має відкритися сторінка «How to play». CANCEL відкриває список ігор, праворуч картинка гри. Саму гру тут не запускай, вона йде менше 1 кадру/с.
4. Опублікуй як **unlisted**.
5. На бейджі: App Store, знайди Pocket Arcade за посиланням, Install.
6. Запусти, потім у консолі бейджа виконай `lua mem`: має бути `app.aot (native)`.
7. Що перевірити очима (5 хвилин):
   - Список: текст під картинкою не налазить на смугу підказки внизу, назви ігор у списку читаються.
   - Alien Invasion, Tobu Tobu Girl DX, uCity: грають плавно.
   - Утримай SELECT+CANCEL 1 с: відкривається меню. Colours міняє кольори, Screen перемикає 1x і 1.5x.
   - HOME посеред гри, потім знову відкрий застосунок: зверху «> назва гри», SELECT продовжує з того самого місця.
   - LED: червоний «вогник живлення» праворуч, верхні 6 світяться кольорами картинки.
8. У консолі кожні 5 с друкується рядок `PASTAT <гра> fps=.. shown=..`. Скинь мені кілька таких рядків для різних ігор: так ми побачимо, яка гра йде повільно.

Якщо щось не так, скинь фото екрана або рядки з консолі, і я виправлю.

## 2. Опублікувати для всіх

Коли перевірка пройшла, зміни видимість на public. Текст для магазину нижче. !LAMP у застосунку немає, тож ручної модерації не буде.

## 3. Репозиторій

Локальний git у `~/Projects/pocket-arcade` з комітами від hypnogaba. На GitHub ще не викладено: скажи, і я створю репозиторій (приватний чи публічний, на твій вибір). GPL-ігри вимагають, щоб їхній код був доступний: посилання на репозиторії є в CREDITS.md і в застосунку (Credits).

## Текст для магазину

**Name:** Pocket Arcade
**Summary:** 30 free Game Boy games in one app, plus your own from the card.
**About:**

30 free Game Boy and Game Boy Color games in one app: shooters, platformers, puzzles, a city builder, a roguelike, card games. Every one is free software, with its makers credited in the app.

- A picture of each game in the list
- HOME keeps your game: it is first in the list next time
- Battery saves, plus a save state for each game
- Six colour palettes for original Game Boy games
- 1x or 1.5x screen, A/B swap
- The LEDs glow with the colours of the game
- Your own games: copy .gb or .gbc files you own to hh/files/pocket-arcade/ on the microSD card

Keys: d-pad, SELECT = A, CANCEL = B, tap both = START, hold both 1 s = menu.
The badge has no speaker, so the games are silent.
Game Boy is a trademark of Nintendo; this app is not made or endorsed by Nintendo.
