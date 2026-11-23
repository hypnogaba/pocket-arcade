# Pocket Arcade 0.2.0: що зробити Івану

Код, ігри й тести готові. Зіп для магазину: `02-Projects/bp26-badge-gameboy/files/pocket-arcade-submit-0.2.0.zip` у vault.

## Що нового в 0.2.0

- **Tank Siege**: наша власна гра-танчики, написана для бейджа (у дусі Battle City, але своя назва, графіка й рівні). 14 рівнів, після них вони повторюються складнішими. Шість бонусів, рекорд і найдальший рівень зберігаються.
- **Potion Panic**: кольоровий платформер у дусі Mario (DevEd, MIT).
- Залишили 15 найсильніших ігор, разом 17. Слабкі головоломки й повтори прибрали.
- Виправлено: опис Shock Lobster не вміщався в три рядки під картинкою.

## 1. Опублікувати приховано і перевірити на бейджі (обовʼязково)

1. Відкрий https://solana.com/breakpoint/badge/apps/submit (акаунт із привʼязаним X).
2. Перетягни зіп або теку `~/Projects/pocket-arcade/app`. Якщо 0.1.0 уже опублікована, це оновлення того самого застосунку.
3. Опублікуй як **unlisted**, постав на бейдж.
4. Запусти, у консолі бейджа виконай `lua mem`: має бути `app.aot (native)`.
5. Що перевірити очима (5 хвилин):
   - Список: першою стоїть Tank Siege, текст під картинкою не налазить на смугу підказки.
   - **Tank Siege**: SELECT починає гру. Хрестовина їздить, SELECT стріляє, торкнутися SELECT+CANCEL разом = пауза. На титулі стрілки вліво/вправо обирають рівень, коли вже пройдено кілька.
   - Чи не надто складно і не надто легко. Перші рівні я налаштував так, щоб простий бот-захисник вигравав приблизно кожну третю гру. Людина має грати краще.
   - Potion Panic, Tobu Tobu Girl DX, uCity: грають плавно.
   - Утримай SELECT+CANCEL 1 с: меню. Screen перемикає 1x і 1.5x (танчики краще дивитися в 1.5x).
   - HOME посеред гри, знову відкрий застосунок: зверху «> назва гри», SELECT продовжує з того самого місця.
6. У консолі кожні 5 с друкується рядок `PASTAT <гра> fps=..`. Скинь мені кілька таких рядків, особливо для `tanks`, `potion` і `tobu`.

Якщо щось не так, скинь фото екрана або рядки з консолі, і я виправлю.

## 2. Опублікувати для всіх

Коли перевірка пройшла, зміни видимість на public. Текст для магазину нижче.

## Текст для магазину

**Name:** Pocket Arcade
**Summary:** 16 free Game Boy games, a new tank game, and your own from the card.
**About:**

Tank Siege, an original tank game made for the badge, plus 16 free Game Boy and Game Boy Color games in one app: platformers, shooters, puzzles, a city builder, a roguelike. Every game is free software, with its makers credited in the app.

- Tank Siege: guard your base from 20 tanks a stage, 14 stages, six bonuses, high score kept
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
