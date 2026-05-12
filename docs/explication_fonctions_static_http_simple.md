# Explication simple des fonctions `static`

Ce document explique les fonctions `static` utilisées dans les deux fichiers HTTP:

- [Core/Src/app_config_http.c](../Core/Src/app_config_http.c)
- [Core/Src/network_config_http.c](../Core/Src/network_config_http.c)

L'objectif est volontairement simple:
- garder le code court,
- garder seulement l'essentiel,
- et montrer avec des exemples concrets ce que chaque fonction reçoit et ce qu'elle produit.

---

## 1) Fichier `app_config_http.c`

### `hex_to_nibble(char c)`

Rôle:
- convertir un caractère hexadécimal en valeur numérique.

Pourquoi:
- utile pour décoder les chaînes URL-encodées comme `%20`.

Exemple:
- input: `'A'`
- output: `10`

- input: `'7'`
- output: `7`

### `url_decode_inplace(char *text)`

Rôle:
- décoder une chaîne HTML/URL directement dans le même buffer.

Pourquoi:
- le navigateur envoie souvent `+` et `%XX` au lieu de texte brut.

Exemple:
- input: `baud%3D19200+test`
- output: `baud=19200 test`

### `get_param(const char *body, const char *key, char *out, unsigned out_size)`

Rôle:
- chercher un champ dans le body POST et copier sa valeur dans `out`.

Pourquoi:
- lire simplement une valeur du formulaire web.

Exemple:
- body: `action=save_port&baud=19200&stop_bits=1`
- key: `baud`
- output: `19200`

### `trim_left(char *text)`

Rôle:
- sauter les espaces au début d'une chaîne.

Pourquoi:
- aider le parseur numérique à rester simple.

Exemple:
- input: `"   123"`
- output: pointeur vers `"123"`

### `parse_ulong(const char *text, unsigned long *value)`

Rôle:
- convertir une chaîne en nombre entier non signé.

Pourquoi:
- lire les nombres envoyés par le formulaire.

Exemple:
- input: `"19200"`
- output: `19200`

- input: `"12abc"`
- output: échec

### `read_unsigned_field(const char *body, const char *key, char *text, unsigned text_size, unsigned long *value)`

Rôle:
- faire en une seule étape: lire le champ puis le parser.

Pourquoi:
- réduire le code du handler `save_port`.

Exemple:
- body: `baud=9600`
- key: `baud`
- output: `9600`

### `page_reset(void)`

Rôle:
- vider le buffer HTML avant de construire une nouvelle page.

Pourquoi:
- éviter de mélanger l'ancienne page avec la nouvelle.

Exemple:
- avant: buffer contient une ancienne page
- après: buffer vide

### `page_append(const char *text)`

Rôle:
- ajouter du texte HTML au buffer final.

Pourquoi:
- construire la page morceau par morceau.

Exemple:
- input: `"<h1>Hello</h1>"`
- output: ce texte est ajouté au buffer HTML

### `page_append_uint(unsigned long value)`

Rôle:
- ajouter un nombre dans le buffer HTML.

Pourquoi:
- afficher des valeurs comme `baud`, `index`, `address`.

Exemple:
- input: `19200`
- output: le texte `19200` dans la page

### `register_type_to_text(registerType_t t)`

Rôle:
- convertir le type Modbus en texte lisible.

Pourquoi:
- afficher `U16`, `I16` ou `FLOAT` dans la page web.

Exemple:
- input: `REG_TYPE_FLOAT`
- output: `FLOAT`

### `send_status_page(struct netconn *conn, int ok, const char *message)`

Rôle:
- envoyer une petite page HTML de résultat.

Pourquoi:
- dire rapidement à l'utilisateur si l'action a réussi ou non.

Exemple:
- input: `ok = 1`, `message = "Port updated and saved"`
- output: page HTTP 200 avec ce message

### `handle_save_port(struct netconn *conn, const char *body)`

Rôle:
- traiter seulement l'action `save_port`.

Pourquoi:
- garder `app_config_http_handle_save()` plus court.

Exemple:
- input body: `action=save_port&baud=19200&stop_bits=1&parity=0&port_id=0`
- output: configuration UART appliquée puis sauvegardée

### `render_port_section(void)`

Rôle:
- construire la partie HTML du port UART.

Exemple:
- input: la config actuelle dans `appDb`
- output: un formulaire HTML avec baud, stop bits, parity

### `render_slave_section(void)`

Rôle:
- construire la partie HTML des esclaves Modbus.

Exemple:
- input: plusieurs esclaves actifs dans `appDb`
- output: tableau HTML avec la liste des esclaves

### `render_register_section(void)`

Rôle:
- construire la partie HTML des registres Modbus.

Exemple:
- input: des registres actifs dans `appDb`
- output: tableau HTML avec les registres et leur type

---

## 2) Fichier `network_config_http.c`

### `parse_ipv4(const char *text, uint8_t out[4])`

Rôle:
- lire une adresse IPv4 comme `192.168.1.10`.

Pourquoi:
- convertir le texte du formulaire en 4 octets.

Exemple:
- input: `"192.168.1.10"`
- output: `out = {192, 168, 1, 10}`

### `hex_to_nibble(char c)`

Rôle:
- convertir un caractère hexadécimal en valeur numérique.

Exemple:
- input: `'F'`
- output: `15`

### `url_decode_inplace(char *text)`

Rôle:
- décoder les caractères `+` et `%XX` dans un texte.

Exemple:
- input: `"192.168.1.10+test"`
- output: `"192.168.1.10 test"`

### `get_param(const char *body, const char *key, char *out, unsigned out_size)`

Rôle:
- lire un champ du body POST.

Exemple:
- body: `ip=192.168.1.10&gateway=192.168.1.1`
- key: `ip`
- output: `192.168.1.10`

---

## 3) Résumé très simple

Dans les deux fichiers:

- les fonctions `static` servent à aider les fonctions principales,
- elles restent locales au fichier,
- elles rendent le code plus simple à lire,
- elles évitent de répéter la même logique.

En pratique:

- `get_param()` lit un champ du formulaire,
- `url_decode_inplace()` nettoie le texte,
- `parse_ulong()` et `parse_ipv4()` transforment le texte en valeur utile,
- `send_status_page()` répond à l'utilisateur,
- `handle_save_port()` garde le POST principal simple.