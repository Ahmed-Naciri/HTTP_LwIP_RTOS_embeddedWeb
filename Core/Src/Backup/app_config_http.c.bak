#include "app_config_http.h"

#include "app_config.h"
#include "main.h"
#include "lwip/sys.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * --- Bibliothèque C et fonctions externes utilisées dans ce fichier ---
 *
 * Ce bloc explique rapidement le rôle des fonctions standard utilisées ci-dessous
 * et donne un exemple court pour chaque cas. Les exemples montrent l'effet
 * attendu (entrée -> sortie) et peuvent être copiés dans un petit programme
 * de test si besoin.
 *
 * 1) snprintf(char *str, size_t size, const char *fmt, ...)
 *    - Fonction sûre pour écrire formaté dans un buffer en limitant la taille.
 *    - Retourne le nombre total de caractères qui auraient été écrits (sans
 *      compter le terminator) ; si >= size, la sortie a été tronquée.
 *    - Exemple:
 *      char b[8];
 *      int n = snprintf(b, sizeof(b), "%s=%d", "x", 123);
 *      // b == "x=123"  (n == 5)
 *
 * 2) strstr(const char *haystack, const char *needle)
 *    - Cherche la première occurrence de "needle" dans "haystack" et retourne
 *      un pointeur vers cette position, ou NULL si non trouvé.
 *    - Exemple:
 *      const char *s = "a=1&b=2";
 *      const char *p = strstr(s, "b=");
 *      // p pointe sur "b=2"
 *
 * 3) strlen(const char *s)
 *    - Retourne la longueur (en octets) d'une chaîne C terminée par '\0'.
 *    - Exemple: strlen("abc") == 3
 *
 * 4) memcpy(void *dest, const void *src, size_t n)
 *    - Copie exactement n octets de src vers dest. Ne gère PAS le chevauchement
 *      (utiliser memmove si chevauchement possible).
 *    - Exemple:
 *      char a[4] = "abc"; memcpy(&a[1], "Z", 1); // a == "aZc"
 *
 * 5) strtoul(const char *nptr, char **endptr, int base)
 *    - Convertit une chaîne en unsigned long ; place en *endptr le pointeur
 *      vers le premier caractère non converti. Utilisé ici pour valider que
 *      toute la chaîne est bien un nombre.
 *    - Exemple:
 *      char *e; unsigned long v = strtoul("123", &e, 10); // v==123, *e=='\0'
 *
 * 6) strcmp(const char *s1, const char *s2)
 *    - Compare deux chaînes ; retourne 0 si égales, <0 si s1<s2, >0 si s1>s2
 *    - Exemple: strcmp("ok","ok") == 0
 *
 * 7) netconn_write(struct netconn *conn, const void *dataptr, size_t size, int apiflags)
 *    - (lwIP) Envoie des octets sur une connexion netconn. Ici utilisé pour
 *      écrire le buffer HTTP complet vers le socket TCP. Comportement dépend
 *      de la pile lwIP utilisée.
 *    - Exemple (conceptuel): netconn_write(conn, buffer, len, NETCONN_COPY);
 *
 * Notes de sécurité et bonnes pratiques:
 * - Toujours fournir des tailles correctes à snprintf pour éviter les
 *   débordements. Vérifier le code de retour si nécessaire.
 * - Quand on copie des octets avec memcpy s'assurer que le buffer destination
 *   a la taille suffisante (ici g_app_config_page_buffer est dimensionné large).
 * - Pour valider une conversion numérique, utiliser strtoul + vérification de
 *   endptr (comme fait dans parse_ulong) plutôt que atoi/atol qui n'ont pas
 *   de signal d'erreur fiable.
 */

/*
 * FICHIER: app_config_http.c
 * OBJECTIF: Gérer l'interface HTTP pour la configuration Modbus RTU
 * 
 * Ce module gère:
 *  - Rendu des pages HTML de configuration (port UART, slaves Modbus, registres)
 *  - Traitement des requêtes POST pour ajouter/supprimer slaves et registres
 *  - Application des changements (baud, parity, stop bits) et sauvegarde en Flash
 *  - Gestion des erreurs avec messages de statut au navigateur
 *
 * Le programme FONCTIONNERA SANS ce fichier, mais sans interface web pour configurer Modbus.
 */

/*
 * VARIABLES GLOBALES:
 *  g_app_config_page_buffer: petit buffer pour petites réponses (statut/messages)
 *  g_page_used: nombre d'octets actuellement utilisés dans le buffer
 * 
 * Ces variables évitent les allocations dynamiques (heap) qui sont risquées en embedded.
 * Sans elles, on devrait allouer/libérer mémoire, ce qui peut causer de la fragmentation.
 */
#define APP_HTTP_SMALL_BUFFER_SIZE 1024u

static char g_app_config_page_buffer[APP_HTTP_SMALL_BUFFER_SIZE];
static size_t g_page_used = 0u;

/*
 * hex_to_nibble(c) . note that nibble is a 4-bit value (0-15) that represents a single hex digit(0-F).
 * 
 * OBJECTIF: Convertir un caractère hexadécimal ('0'-'9', 'A'-'F', 'a'-'f') en sa valeur numérique (0-15)
 * 
 * POURQUOI: Nécessaire pour décoder les URL encodées comme %20 (espace), %3D (=), etc.
 *           Les formulaires web envoient des données URL-encodées, pas du texte brut.
 * 
 * UTILISATION: Appelée par url_decode_inplace() pour décoder chaque couple de hex.
 * 
 * SANS CETTE FONCTION: Les données du formulaire ne seraient pas décodées correctement.
 *                      Les valeurs comme "19200" resteraient encodées => erreur de parsing.
 */
static int hex_to_nibble(char c)
{
  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  }
  if ((c >= 'A') && (c <= 'F')) {
    return c - 'A' + 10;
  }
  if ((c >= 'a') && (c <= 'f')) {
    return c - 'a' + 10;
  }
  return -1;
}

/*
 * url_decode_inplace(text)
 * 
 * OBJECTIF: Décoder une chaîne URL-encodée IN PLACE (sans allocations supplémentaires)
 * 
 * MÉCANISME:
 *  - Remplace '+' par espace
 *  - Remplace '%XY' par le caractère correspondant (X,Y = hex digits)
 *  - Autres caractères restent inchangés
 * 
 * POURQUOI: Les navigateurs envoient les données de formulaire au format URL-encoded:
 *           form field "baud=19200" devient "baud%3D19200" si contient caractères spéciaux.
 *           Sans décodage, on ne peut pas extraire les valeurs correctement.
 * 
 * UTILISATION: Appelée par get_param() après extraction du champ pour nettoyer la valeur.
 * 
 * SANS CETTE FONCTION: Les valeurs extraites restent encodées (ex: "19200" au lieu de "19200")
 *                      => Les parsers numériques échoueraient => Configuration impossible.
 */
static void url_decode_inplace(char *text)
{
  char *src = text;
  char *dst = text;
  int hi;
  int lo;

  while (*src != '\0') {
    if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else if ((*src == '%') && (src[1] != '\0') && (src[2] != '\0')) {
      hi = hex_to_nibble(src[1]);
      lo = hex_to_nibble(src[2]);
      if ((hi >= 0) && (lo >= 0)) {
        *dst++ = (char)((hi << 4) | lo);
        src += 3;
      } else {
        *dst++ = *src++; 
      }
    } else {
      *dst++ = *src++;
    }
  }

  *dst = '\0';
}

/*
 * get_param(body, key, out, out_size)
 * 
 * OBJECTIF: Extraire la valeur d'un paramètre nommé du corps HTTP POST (format: key1=val1&key2=val2&...)
 * 
 * PARAMÈTRES:
 *  - body: chaîne POST (ex: "baud=19200&parity=0&stop_bits=1")
 *  - key: nom du paramètre à chercher (ex: "baud")
 *  - out: buffer de sortie pour la valeur
 *  - out_size: taille max du buffer
 * 
 * RETOUR: 1 si trouvé, 0 sinon
 * 
 * MÉCANISME:
 *  1. Cherche "key=" dans le body
 *  2. Extrait la valeur jusqu'à '&' ou fin de chaîne
 *  3. Décode l'URL (remplace %XX par caractères)
 *  4. Supprime les espaces de fin (trim)
 * 
 * POURQUOI: Les requêtes POST contiennent plusieurs paramètres. On doit les extraire individuellement.
 *           Par exemple, pour appliquer une config UART, on extrait baud, parity, stop_bits séparément.
 * 
 * UTILISATION: Appelée dans app_config_http_handle_save() pour lire chaque champ du formulaire.
 * 
 * SANS CETTE FONCTION: On ne pourrait pas lire les données du formulaire => Impossible de configurer le MCU via web.
 */
static int get_param(const char *body, const char *key, char *out, unsigned out_size)
{
  char pattern[40];
  const char *k;
  const char *v;
  unsigned i = 0;

  if ((body == NULL) || (key == NULL) || (out == NULL) || (out_size == 0u)) {
    return 0;
  }

  (void)snprintf(pattern, sizeof(pattern), "%s=", key);
  k = strstr(body, pattern);
  if (k == NULL) {
    return 0;
  }

  v = k + strlen(pattern);
  while ((v[i] != '\0') && (v[i] != '&') && (i < (out_size - 1u))) {
    out[i] = v[i];
    i++;
  }
  out[i] = '\0';

  url_decode_inplace(out);

  while ((i > 0u) &&
         ((out[i - 1u] == ' ') || (out[i - 1u] == '\t') || (out[i - 1u] == '\r') ||
          (out[i - 1u] == '\n'))) {
    out[i - 1u] = '\0';
    i--;
  }

  return 1;
}

/* Build a POST field name with indices, then read it. */
static int get_indexed_param(const char *body, const char *base, unsigned long a, unsigned long b,
                             char *out, unsigned out_size)
{
  char key[48];

  if ((body == NULL) || (base == NULL) || (out == NULL) || (out_size == 0u)) {
    return 0;
  }

  (void)snprintf(key, sizeof(key), "%s_%lu_%lu", base, a, b);
  return get_param(body, key, out, out_size);
}

/*
 * trim_left(text)
 * 
 * OBJECTIF: Sauter les espaces au début d'une chaîne (espace, tab, carriage return)
 * 
 * RETOUR: Pointeur vers le premier caractère non-espace
 * 
 * POURQUOI: Après extraction d'un paramètre, il peut y avoir des espaces. Cette fonction
 *           aide à nettoyer la chaîne avant de vérifier si elle est vide ou invalide.
 * 
 * UTILISATION: Appelée par parse_ulong() pour vérifier qu'il n'y a pas de caractères
 *              de fin après le nombre (validation stricte).
 * 
 * SANS CETTE FONCTION: parse_ulong() accepterait des chaînes avec des espaces au début
 *                      (moins strict, mais probablement acceptable). Les validations seraient moins fiables.
 */
static char *trim_left(char *text)
{
  while ((*text == ' ') || (*text == '\t') || (*text == '\r')) {
    text++;
  }
  return text;
}
/*
 * parse_ulong(text, value)
 * 
 * OBJECTIF: Parser une chaîne décimale en nombre non-signé long avec validation stricte
 * 
 * PARAMÈTRES:
 *  - text: chaîne à parser (ex: "19200")
 *  - value: pointeur pour stocker le résultat
 * 
 * RETOUR: 1 si succès (nombre valide, pas de caractères de fin invalides), 0 sinon
 * 
 * VALIDATION STRICTE:
 *  - Accepte seulement des chiffres décimaux
 *  - Rejette si caractères de fin non-espaces (ex: "19200abc" -> rejeté)
 *  - Réjectionnels les chaînes vides
 * 
 * POURQUOI: Les valeurs du formulaire (baud, adresse esclave, registre) doivent être des nombres valides.
 *           Une validation stricte empêche les entrées malveillantes ou mal formatées.
 * 
 * UTILISATION: Appélée dans les actions POST (save_port, add_slave, add_register) pour valider
 *              chaque champ numérique avant de l'utiliser.
 * 
 * SANS CETTE FONCTION: Les entrées non-numériques seraient acceptées par strtoul() direct
 *                      => Comportements imprévisibles, erreurs de configuration => DANGEREUX.
 */
static int parse_ulong(const char *text, unsigned long *value)
{
  char *endptr;

  if ((text == NULL) || (value == NULL)) {
    return 0;
  }

  *value = strtoul(text, &endptr, 10);
  if ((endptr == text) || (*trim_left(endptr) != '\0')) {
    return 0;
  }

  return 1;
}

/* Read one POST field and parse it as a float with strict validation. */
static int parse_float_value(const char *text, float *value)
{
  char *endptr;

  if ((text == NULL) || (value == NULL)) {
    return 0;
  }

  *value = strtof(text, &endptr);
  if ((endptr == text) || (*trim_left(endptr) != '\0')) {
    return 0;
  }

  return 1;
}

/* Return the register matching a slave index and register address. */
static registerConfig_t *find_register_config(uint8_t slaveIndex, uint16_t regAddress)
{
  uint8_t i;

  if (slaveIndex >= MAX_SLAVES) {
    return 0;
  }

  if (appDb.slaveConfig[slaveIndex].used != 1u) {
    return 0;
  }

  for (i = 0; i < MAX_REGISTERS_PER_SLAVE; i++) {
    if ((appDb.slaveConfig[slaveIndex].registerConfig[i].used == 1u) &&
        (appDb.slaveConfig[slaveIndex].registerConfig[i].regAddress == regAddress)) {
      return &appDb.slaveConfig[slaveIndex].registerConfig[i];
    }
  }

  return 0;
}

/* Read one POST field and parse it as an unsigned number. */
static int read_unsigned_field(const char *body, const char *key, char *text, unsigned text_size,
							   unsigned long *value)
{
  if (!get_param(body, key, text, text_size)) {
    return 0;
  }

  return parse_ulong(text, value);
}

/*
 * page_reset()
 * 
 * OBJECTIF: Réinitialiser le buffer HTML global pour une nouvelle page
 * 
 * MÉCANISME:
 *  - Réinitialise g_page_used à 0 (marque le buffer comme vide)
 *  - Nettoit le premier caractère (null terminator)
 * 
 * POURQUOI: Chaque fois qu'on doit générer une nouvelle page (form, réponse statut),
 *           on doit commencer avec un buffer vide. Sans cela, les anciennes données resteraient
 *           et s'ajouteraient à la nouvelle page => corruption du contenu.
 * 
 * UTILISATION: Appelée au début de chaque fonction qui génère une page HTML
 *              (app_config_http_send_form, send_status_page).
 * 
 * SANS CETTE FONCTION: Les pages s'accumuleraient les unes sur les autres
 *                      => HTML invalide, affichage cassé dans le navigateur.
 *
 */
static void page_reset(void)
{
  g_page_used = 0u;
  g_app_config_page_buffer[0] = '\0'; 
}

/*
 * page_append(text)
 * 
 * OBJECTIF: Ajouter du texte brut au buffer HTML sans dépassement
 * 
 * MÉCANISME:
 *  - Vérifie l'espace disponible dans le buffer (12000 octets total)
 *  - Copie le texte avec memcpy (sécurisé)
 *  - Accumule g_page_used (pointeur de fin)
 * 
 * POURQUOI: On doit construire la page HTML morceau par morceau (enêtes, balises, données).
 *           Sans cette fonction, on devrait allouer/libérer mem pour chaque fragment.
 *           Utiliser un buffer statique évite la fragmentation du heap en embedded.
 * 
 * UTILISATION: Appelée partout pour ajouter des éléments HTML (tags, texte litéral).
 * 
 * SANS CETTE FONCTION: Impossible de construire HTML => Pas de page web du tout.
 */
static void page_append(const char *text)
{
  size_t text_len;
  size_t remaining;

  if (text == NULL) {
    return;
  }

  if (g_page_used >= (sizeof(g_app_config_page_buffer) - 1u)) {
    return;
  }

  text_len = strlen(text);
  remaining = (sizeof(g_app_config_page_buffer) - 1u) - g_page_used;
  if (text_len > remaining) {
    text_len = remaining;
  }

  if (text_len > 0u) {
    (void)memcpy(&g_app_config_page_buffer[g_page_used], text, text_len);
    g_page_used += text_len;
    g_app_config_page_buffer[g_page_used] = '\0';
  }
}

/*
 * page_append_uint(value)
 * 
 * OBJECTIF: Ajouter un nombre entier non-signé au buffer HTML sans utiliser sprintf
 * 
 * MÉCANISME:
 *  - Décompose le nombre en chiffres (unitsés, dizianes, etc.) en inversant l'ordre
 *  - Réassemble les chiffres dans le bon ordre et les ajoute au buffer
 * 
 * POURQUOI: Cette fonction est optimisée pour l'embedded:
 *           - Pas d'appel à sprintf (lourd, peut utiliser beaucoup de stack)
 *           - Opérations simples: division, modulo, caractères ASCII
 *           - Utilisée fréquemment pour afficher adresses, valeurs, indices
 * 
 * UTILISATION: Appelée partout dans le rendu HTML pour afficher des nombres
 *              (baud rate, slave address, register index, etc.).
 * 
 * SANS CETTE FONCTION: On devrait utiliser sprintf (coûteux) ou construire des strings manuellement
 *                      => Code moins efficace, plus de consommation RAM/CPU.
 */
static void page_append_uint(unsigned long value)
{
  char tmp[11];
  size_t i = 0u;
  size_t j;

  if (value == 0ul) {
    page_append("0");
    return;
  }

  while ((value > 0ul) && (i < sizeof(tmp))) {
    tmp[i] = (char)('0' + (value % 10ul));
    value /= 10ul;
    i++;
  }

  for (j = i; j > 0u; j--) {
    char c[2];
    c[0] = tmp[j - 1u];
    c[1] = '\0';
    page_append(c);
  }
}

static const char *slave_display_name(uint8_t slaveIndex)
{
  static char fallback[MAX_SLAVE_NAME_LEN];

  if ((slaveIndex >= MAX_SLAVES) || (appDb.slaveConfig[slaveIndex].used != 1u)) {
    return "";
  }

  if ((memchr(appDb.slaveConfig[slaveIndex].slaveName, '\0', MAX_SLAVE_NAME_LEN) != 0) &&
      (appDb.slaveConfig[slaveIndex].slaveName[0] != '\0')) {
    return appDb.slaveConfig[slaveIndex].slaveName;
  }

  (void)snprintf(fallback, sizeof(fallback), "Slave %u", (unsigned int)(slaveIndex + 1u));
  return fallback;
}

static void page_append_html_escaped(const char *text)
{
  if (text == NULL) {
    return;
  }

  while (*text != '\0') {
    switch (*text) {
      case '&':
        page_append("&amp;");
        break;
      case '<':
        page_append("&lt;");
        break;
      case '>':
        page_append("&gt;");
        break;
      case '"':
        page_append("&quot;");
        break;
      case '\'':
        page_append("&#39;");
        break;
      default: {
        char c[2];
        c[0] = *text;
        c[1] = '\0';
        page_append(c);
        break;
      }
    }
    text++;
  }
}

/*
 * register_type_to_text(t)
 * 
 * OBJECTIF: Convertir un type de registre Modbus (enum registerType_t) en chaîne de texte lisible
 * 
 * PARAMÈTRES:
 *  - t: type de registre (REG_TYPE_U16, REG_TYPE_I16, REG_TYPE_FLOAT)
 * 
 * RETOUR: Pointeur vers une chaîne statique ("U16", "I16", "FLOAT")
 * 
 * POURQUOI: La page web affiche les types de registres sous forme de texte lisible pour l'utilisateur.
 *           Cette fonction fait la traduction du code interne vers l'affichage.
 * 
 * UTILISATION: Appelée par render_register_section() pour afficher le type de chaque registre
 *              dans le tableau HTML.
 * 
 * SANS CETTE FONCTION: Afficherait des nombres (0, 1, 2) au lieu de noms lisibles
 *                      => L'utilisateur ne comprendrait pas quel type est actif.
 */
static const char *register_type_to_text(registerType_t t)
{
  if (t == REG_TYPE_U16) {
    return "U16";
  }
  if (t == REG_TYPE_I16) {
    return "I16";
  }
  return "FLOAT";
}

/*
 * send_status_page(conn, ok, message)
 * 
 * OBJECTIF: Envoyer une page de statut simple au navigateur après une action POST
 * 
 * PARAMÈTRES:
 *  - conn: connexion netconn (socket TCP)
 *  - ok: 1 = succès (200 OK), 0 = échec (400 Bad Request)
 *  - message: texte explicatif à afficher à l'utilisateur
 * 
 * MÉCANISME:
 *  1. Réinitialise le buffer de page
 *  2. Génère les en-têtes HTTP (statut, Content-Type)
 *  3. Génère le corps HTML avec le message et un lien pour revenir à la config
 *  4. Envoie le tout au client via netconn_write()
 * 
 * POURQUOI: Après chaque action (save_port, add_slave, etc.), le navigateur attend une réponse.
 *           Au lieu de générer la page entière de config, on envoie un court message de statut.
 *           C'est plus rapide et l'utilisateur a un feedback clair (succès ou erreur).
 * 
 * UTILISATION: Appelée dans app_config_http_handle_save() pour répondre à chaque action POST.
 * 
 * SANS CETTE FONCTION: Pas de feedback HTTP => Le navigateur attendrait le timeout
 *                      => Mauvaise UX, l'utilisateur ne saurait pas si l'action a marche.
 */
static void send_status_page(struct netconn *conn, int ok, const char *message)
{
  page_reset();
  page_append("HTTP/1.1 ");
  page_append(ok ? "200 OK\r\n" : "400 Bad Request\r\n");
  page_append("Content-Type: text/html\r\nConnection: close\r\n\r\n");
  page_append("<!doctype html><html><head><meta charset=\"utf-8\"><title>Modbus</title></head>");
    page_append("<body style=\"font-family:Arial,sans-serif;max-width:900px;margin:20px auto;\">");
    page_append("<body style=\"font-family:Arial,sans-serif;max-width:900px;margin:20px auto;\">");
  page_append(ok ? "<h1>Operation completed</h1>" : "<h1>Operation failed</h1>");
  page_append("<p>");
  page_append((message != NULL) ? message : "No details");
  page_append("</p>");
  page_append("<p><a href=\"/modbus_config.html\">Back to Modbus configuration</a></p>");
  page_append("</body></html>");

  netconn_write(conn, g_app_config_page_buffer, g_page_used, NETCONN_COPY);
}

/* Handle only the UART save action so the main POST handler stays short. */
static void handle_save_port(struct netconn *conn, const char *body)
{
  char v_baud[20];
  char v_stop[8];
  char v_parity[8];
  char v_port[8];
  unsigned long baud;
  unsigned long stop_bits;
  unsigned long parity;
  unsigned long port_id;

  if ((!read_unsigned_field(body, "baud", v_baud, sizeof(v_baud), &baud)) ||
	  (!read_unsigned_field(body, "stop_bits", v_stop, sizeof(v_stop), &stop_bits)) ||
	  (!read_unsigned_field(body, "parity", v_parity, sizeof(v_parity), &parity)) ||
	  (!read_unsigned_field(body, "port_id", v_port, sizeof(v_port), &port_id))) {
    send_status_page(conn, 0, "Port fields are missing or invalid");
    return;
  }

  if ((port_id >= (unsigned long)MAX_UART_PORTS) || (baud == 0ul) || (baud > 2000000ul) ||
	  ((stop_bits != 1ul) && (stop_bits != 2ul)) || (parity > (unsigned long)PARITY_ODD)) {
    send_status_page(conn, 0, "Port values are out of range");
    return;
  }

  if (appConfig_updatePort((uartPortId_t)port_id, (uint32_t)baud, (uint8_t)stop_bits,
					   (parityType_t)parity) < 0) {
    send_status_page(conn, 0, "Port update failed");
    return;
  }

  if (appConfig_save() < 0) {
    send_status_page(conn, 0, "Save failed after port update");
    return;
  }

  send_status_page(conn, 1, "Port updated and saved");
}

/*
 * render_port_section()
 * 
 * OBJECTIF: Générer la section HTML de configuration du port UART dans la page Modbus config
 * 
 * MÉCANISME:
 *  1. Affiche un titre <h2>Port</h2>
 *  2. Crée un formulaire POST avec action="/save_modbus_config" et action="save_port"
 *  3. Champs du formulaire:
 *     - Dropdown pour choisir le port (PORT_ID_UART1, PORT_ID_UART2, etc.)
 *     - Input pour le baud rate (ex: 9600, 19200)
 *     - Input pour stop bits (1 ou 2)
 *     - Input pour parity (0=None, 1=Even, 2=Odd)
 *  4. Remplit les valeurs actuelles depuis appDb.ports[0]
 *  5. Ajoute un bouton "Save port"
 * 
 * POURQUOI: L'utilisateur doit pouvoir configurer les paramètres UART (baud, parity, etc.)
 *           via l'interface web. Cette fonction génère l'UI pour cela.
 *           Une fois soumis, le POST est traité par app_config_http_handle_save().
 * 
 * UTILISATION: Appelée par app_config_http_send_form() pour construire la page complète.
 * 
 * SANS CETTE FONCTION: L'utilisateur ne pourrait pas changer baud/parity depuis le web
 *                      => Faudrait ré-programmer le MCU pour chaque changement de config => IMPOSSIBLE.
 */
static void render_port_section(void)
{
  uint8_t i;

  /* Keep the global save action documented but disabled in the UI. */
//   page_append("<p><form method=\"POST\" action=\"/save_modbus_config\" style=\"margin:0 0 12px 0\">"
//               "<input type=\"hidden\" name=\"action\" value=\"save_all\">"
//               "<button type=\"submit\">Save all changes to Flash</button>"
//               "</form></p>");

  page_append("<h2>Port</h2>");
  page_append("<form method=\"POST\" action=\"/save_modbus_config\">\n");
  page_append("<input type=\"hidden\" name=\"action\" value=\"save_port\">\n");
  page_append("<p><label>Port<br><select name=\"port_id\">");
  for (i = 0; i < MAX_UART_PORTS; i++) {
    page_append("<option value=\"");
    page_append_uint((unsigned long)i);
    page_append("\" ");
    if (appDb.ports[0].portId == (uartPortId_t)i) { 
      page_append("selected");
    }
    page_append(">UART");
    page_append_uint((unsigned long)(i + 1u));
    page_append("</option>");
  }
  page_append("</select></label></p>");

  page_append("<p><label>Baud rate<br><input name=\"baud\" value=\"");
  page_append_uint((unsigned long)appDb.ports[0].baudRate);
  page_append("\" style=\"width:220px\"></label></p>");

  page_append("<p><label>Stop bits (1 or 2)<br><input name=\"stop_bits\" value=\"");
  page_append_uint((unsigned long)appDb.ports[0].stopBits);
  page_append("\" style=\"width:220px\"></label></p>");

  page_append("<p><label>Parity (0=None,1=Even,2=Odd)<br><input name=\"parity\" value=\"");
  page_append_uint((unsigned long)appDb.ports[0].parity);
  page_append("\" style=\"width:220px\"></label></p>");
  page_append("<p><button type=\"submit\">Save port</button></p></form>");

  if (MAX_UART_PORTS == 1u) {
    page_append("<p style=\"color:#666\">Runtime is currently wired to one UART in firmware.</p>");
  }
}

/*
 * render_slave_section()
 * 
 * OBJECTIF: Générer la section HTML pour afficher/gérer les esclaves Modbus
 * 
 * MÉCANISME:
 *  1. Affiche un titre <h2>Slaves</h2>
 *  2. Crée un tableau HTML listant tous les esclaves actifs (appDb.slaveConfig[i].used == 1)
 *     - Colonnes: Index, Address, Port, Registers, Action (Delete button)
 *  3. Ajoute une section "Add slave" avec formulaire POST:
 *     - Input pour l'adresse esclave (1..247)
 *     - Dropdown pour le port UART
 *  4. Bouton "Add slave"
 * 
 * POURQUOI: L'utilisateur doit pouvoir ajouter/supprimer des esclaves Modbus dynamiquement.
 *           La liste affiche l'état actuel, et le formulaire permet l'ajout.
 *           Chaque suppression appelle l'action "delete_slave" qui nettoie aussi les registres
 *           associés (cascade cleanup).
 * 
 * UTILISATION: Appelée par app_config_http_send_form() pour construire la page complète.
 * 
 * SANS CETTE FONCTION: L'utilisateur ne pourrait pas créer de slaves sans ré-programmer le MCU
 *                      => Fonctionnalité Modbus inaccessible depuis le web => INUTILE.
 */
static void render_slave_section(void)
{
  uint8_t i;

  page_append("<h2>Slaves</h2>");
  page_append("<table border=\"1\" cellpadding=\"6\" cellspacing=\"0\" style=\"border-collapse:collapse;width:100%\">"
              "<tr><th>Name</th><th>Address</th><th>Port</th><th>Registers</th><th>Action</th></tr>");
  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      page_append("<tr>");
      page_append("<td>");
      page_append_html_escaped(slave_display_name(i));
      page_append("</td>");
      page_append("<td>");
      page_append_uint((unsigned long)appDb.slaveConfig[i].slaveAddress);
      page_append("</td>");
      page_append("<td>UART");
      page_append_uint((unsigned long)appDb.slaveConfig[i].portId + 1u);
      page_append("</td>");
      page_append("<td>");
      page_append_uint((unsigned long)appDb.slaveConfig[i].registerCount);
      page_append("</td>");
      page_append("<td><form method=\"POST\" action=\"/save_modbus_config\" style=\"margin:0\">"
                  "<input type=\"hidden\" name=\"action\" value=\"delete_slave\">");
      page_append("<input type=\"hidden\" name=\"slave_index\" value=\"");
      page_append_uint((unsigned long)i);
      page_append("\">");
      page_append("<button type=\"submit\">Delete slave</button></form></td>");
      page_append("</tr>");
    }
  }
  page_append("</table>");

  page_append("<h3>Add slave</h3>");
  page_append("<form method=\"POST\" action=\"/save_modbus_config\">"
              "<input type=\"hidden\" name=\"action\" value=\"add_slave\">"
              "<p><label>Slave name<br><input name=\"slave_name\" maxlength=\"23\" style=\"width:220px\" required></label></p>"
              "<p><label>Slave address (1..247)<br><input name=\"slave_address\" style=\"width:220px\"></label></p>"
              "<p><label>Port<br><select name=\"slave_port\">");
  for (i = 0; i < MAX_UART_PORTS; i++) {
    page_append("<option value=\"");
    page_append_uint((unsigned long)i);
    page_append("\">UART");
    page_append_uint((unsigned long)(i + 1u));
    page_append("</option>");
  }
  page_append("</select></label></p><p><button type=\"submit\">Add slave</button></p></form>");
}

/*
 * render_register_section()
 * 
 * OBJECTIF: Générer la section HTML pour afficher/gérer les registres Modbus des esclaves
 * 
 * MÉCANISME:
 *  1. Affiche un titre <h2>Registers</h2>
 *  2. Crée un tableau HTML listant tous les registres (parcourt esclaves et leurs registres)
 *     - Colonnes: Slave index, Slave address, Register address, Type, Action (Delete button)
 *     - N'affiche que les registres avec used == 1
 *  3. Ajoute une section "Add register" avec formulaire POST:
 *     - Dropdown pour choisir l'esclave
 *     - Input pour l'adresse du registre (0..65535)
 *     - Dropdown pour le type (U16, I16, FLOAT)
 *  4. Message d'avertissement si aucun esclave n'existe
 * 
 * POURQUOI: Un esclave Modbus est inutile sans registres à lire/écrire. Cette fonction
 *           permet à l'utilisateur de configurer les registres de chaque esclave via le web.
 *           Chaque registre correspond à une adresse de holding register ou input register
 *           que le master Modbus va interroger.\n *
 * UTILISATION: Appelée par app_config_http_send_form() pour construire la page complète.
 * 
 * SANS CETTE FONCTION: L'utilisateur ne pourrait pas mapper les registres Modbus
 *                      => Impossible de lire/écrire des données depuis le réseau => INUTILE.
 */
static void render_register_section(void)
{
  uint8_t i;
  uint8_t j;
  int has_slave = 0;

  page_append("<h2>Registers</h2>");
  page_append("<table border=\"1\" cellpadding=\"6\" cellspacing=\"0\" style=\"border-collapse:collapse;width:100%\">"
              "<tr><th>Slave name</th><th>Slave address</th><th>Register address</th><th>Type</th><th>Action</th></tr>");

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used == 1u) {
          page_append("<tr>");
          page_append("<td>");
          page_append_html_escaped(slave_display_name(i));
          page_append("</td>");
          page_append("<td>");
          page_append_uint((unsigned long)appDb.slaveConfig[i].slaveAddress);
          page_append("</td>");
          page_append("<td>");
          page_append_uint((unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);
          page_append("</td>");
          page_append("<td>");
          page_append(register_type_to_text(appDb.slaveConfig[i].registerConfig[j].registerType));
          page_append("</td>");
          page_append("<td><form method=\"POST\" action=\"/save_modbus_config\" style=\"margin:0\">"
                      "<input type=\"hidden\" name=\"action\" value=\"delete_register\">");
          page_append("<input type=\"hidden\" name=\"reg_slave_index\" value=\"");
          page_append_uint((unsigned long)i);
          page_append("\">");
          page_append("<input type=\"hidden\" name=\"reg_address\" value=\"");
          page_append_uint((unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);
          page_append("\">");
          page_append("<button type=\"submit\">Delete register</button></form></td>");
          page_append("</tr>");
        }
      }
    }
  }
  page_append("</table>");

  page_append("<h3>Add register</h3>");
  page_append("<form method=\"POST\" action=\"/save_modbus_config\">"
              "<input type=\"hidden\" name=\"action\" value=\"add_register\">"
              "<p><label>Slave<br><select name=\"reg_slave_index\">");

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      has_slave = 1;
      page_append("<option value=\"");
      page_append_uint((unsigned long)i);
      page_append("\">");
      page_append_html_escaped(slave_display_name(i));
      page_append(" - addr ");
      page_append_uint((unsigned long)appDb.slaveConfig[i].slaveAddress);
      page_append("</option>");
    }
  }
  page_append("</select></label></p>");

  page_append("<p><label>Register address (0..65535)<br><input name=\"reg_address\" style=\"width:220px\"></label></p>");
  page_append("<p><label>Type<br><select name=\"reg_type\">"
              "<option value=\"0\">U16</option>"
              "<option value=\"1\">I16</option>"
              "<option value=\"2\">FLOAT</option>"
              "</select></label></p>");

  if (has_slave == 0) {
    page_append("<p style=\"color:#900\">No active slave. Add a slave first.</p>");
  }

  page_append("<p><button type=\"submit\">Add register</button></p></form>");
}

/*
 * app_config_http_send_form(conn)
 * 
 * OBJECTIF: Construire et envoyer la page HTML complète de configuration Modbus au navigateur
 * 
 * PARAMÈTRES:
 *  - conn: connexion netconn (socket TCP) vers le client
 * 
 * MÉCANISME:
 *  1. Réinitialise le buffer HTML
 *  2. Ajoute les en-têtes HTTP (statut 200, Content-Type: text/html, Connection: close)
 *  3. Ajoute le squelette HTML (doctype, head, body)
 *  4. Appelle les trois fonctions render_*_section() pour construire:
 *     - Section Port (configuration UART)
 *     - Section Slaves (liste + ajout)
 *     - Section Registers (liste + ajout)
 *  5. Ajoute des liens de navigation (vers network config, main page)
 *  6. Envoie tout au client via netconn_write()
 * 
 * POURQUOI: C'est le point d'entrée principal pour générer la page de configuration Modbus.
 *           Cette fonction est appelée quand le client demande /modbus_config.html\n *           Elle affiche l'état actuel de la config (ports, esclaves, registres) et
 *           fournit les formulaires pour les modifier.\n *
 * FLUX:\n *  - app_config_http_send_form() est appelée par le serveur HTTP (httpserver-netconn.c)
 *           quand le client demande /modbus_config.html\n *  - Elle génère une page avec l'état actuel via les render_*_section()\n *  - Les utilisateurs peuvent remplir les formulaires et cliquer \"Submit\"\n *  - Les POSTs sont traités par app_config_http_handle_save()\n * \n * UTILISATION: Appelée directement par le serveur HTTP lwIP (netconn).\n * \n * SANS CETTE FONCTION: Pas de page de configuration web pour Modbus\n *                      => Les utilisateurs ne pourraient pas gérer les esclaves/registres\n *                      => PRODUIT INUTILISABLE.\n */
static void http_write(struct netconn *conn, const char *s)
{
  if ((s == NULL) || (conn == NULL)) {
    return;
  }

  {
    const char *p = s;
    size_t remaining = strlen(s);

    while (remaining > 0u) {
      size_t chunk = (remaining > 256u) ? 256u : remaining;
      err_t err;
      uint16_t retry = 0u;

      do {
        err = netconn_write(conn, p, chunk, NETCONN_COPY);
        if (err == ERR_MEM) {
          /* Let TCP task drain queued segments before retrying. */
          sys_msleep(2u);
        }
        retry++;
      } while ((err == ERR_MEM) && (retry < 200u));

      if (err != ERR_OK) {
        /* Stop on persistent socket error; caller will close connection. */
        break;
      }

      p += chunk;
      remaining -= chunk;
    }
  }
}

static void http_write_uint(struct netconn *conn, unsigned long v)
{
  char buf[12];
  int n = snprintf(buf, sizeof(buf), "%lu", v);
  if (n > 0) {
    http_write(conn, buf);
  }
}


/* Helper: stream float value with fixed precision without using printf float support */
static void http_write_float(struct netconn *conn, float v)
{
  int neg = 0;
  if (v < 0.0f) {
    neg = 1;
    v = -v;
  }

  unsigned long ip = (unsigned long)v;
  float fracf = v - (float)ip;
  unsigned long frac = (unsigned long)(fracf * 1000.0f + 0.5f);
  if (frac >= 1000u) {
    ip += 1u;
    frac -= 1000u;
  }

  if (neg) {
    http_write(conn, "-");
  }

  http_write_uint(conn, ip);
  http_write(conn, ".");

  char fbuf[4];
  fbuf[0] = '0' + (char)((frac / 100u) % 10u);
  fbuf[1] = '0' + (char)((frac / 10u) % 10u);
  fbuf[2] = '0' + (char)(frac % 10u);
  fbuf[3] = '\0';
  http_write(conn, fbuf);
}

static void http_write_html_escaped(struct netconn *conn, const char *text)
{
  if ((conn == NULL) || (text == NULL)) {
    return;
  }

  while (*text != '\0') {
    switch (*text) {
      case '&':
        http_write(conn, "&amp;");
        break;
      case '<':
        http_write(conn, "&lt;");
        break;
      case '>':
        http_write(conn, "&gt;");
        break;
      case '"':
        http_write(conn, "&quot;");
        break;
      case '\'':
        http_write(conn, "&#39;");
        break;
      default: {
        char c[2];
        c[0] = *text;
        c[1] = '\0';
        http_write(conn, c);
        break;
      }
    }
    text++;
  }
}

static void http_write_json_escaped(struct netconn *conn, const char *text)
{
  if ((conn == NULL) || (text == NULL)) {
    return;
  }

  while (*text != '\0') {
    switch (*text) {
      case '\\':
        http_write(conn, "\\\\");
        break;
      case '"':
        http_write(conn, "\\\"");
        break;
      case '\b':
        http_write(conn, "\\b");
        break;
      case '\f':
        http_write(conn, "\\f");
        break;
      case '\n':
        http_write(conn, "\\n");
        break;
      case '\r':
        http_write(conn, "\\r");
        break;
      case '\t':
        http_write(conn, "\\t");
        break;
      default: {
        char c[2];
        c[0] = *text;
        c[1] = '\0';
        http_write(conn, c);
        break;
      }
    }
    text++;
  }
}

/* PORT CONFIGURATION PAGE - Lightweight, fast */
void app_config_http_send_port_form(struct netconn *conn)
{
  uint8_t i;
  uint8_t current_port = 0u;
  static const unsigned long baud_rates[] = {9600ul, 14400ul, 19200ul, 38400ul, 56000ul, 57600ul, 115200ul};
  uint8_t baud_index;
  uint8_t current_baud_found = 0u;

  if (appDb.ports[0].portId < (uartPortId_t)MAX_UART_PORTS) {
    current_port = (uint8_t)appDb.ports[0].portId;
  }

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>UART Port Configuration</title>"
    "<link href='https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined:wght,FILL@100..700,0..1&display=swap' rel='stylesheet'/>"
    "<style>"
    ":root{--primary:#0b5dbb;--primary2:#0e6bd8;--surface:#f5f7fa;--card:#ffffff;--soft:#eef2f7;--text:#18222d;--muted:#697386;--border:#d7dde6;--danger:#b00020;--radius:24px;--shadow:0 8px 24px rgba(18,38,63,.08);}"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{min-height:100vh;background:var(--surface);color:var(--text);display:flex;font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;}"
    "a{text-decoration:none;color:inherit;}"
    ".sidebar{width:256px;background:#eef3f8;border-right:1px solid rgba(0,0,0,.06);padding:18px 16px;display:flex;flex-direction:column;gap:18px;height:100vh;position:sticky;top:0;}"
    ".brand h1{font-size:22px;line-height:1.1;font-weight:900;letter-spacing:-.03em;color:#0d4f99;text-transform:uppercase;}"
    ".brand p{font-size:12px;color:var(--muted);margin-top:2px;font-weight:600;}"
    ".nav{display:flex;flex-direction:column;gap:6px;flex:1;}"
    ".nav-link{display:flex;align-items:center;gap:12px;padding:12px 14px;border-radius:14px;color:#556070;font-weight:600;font-size:14px;}"
    ".nav-link.active{background:#fff;color:#0d4f99;box-shadow:0 1px 3px rgba(0,0,0,.08);}"
    ".nav-link .icon{width:18px;text-align:center;font-size:15px;}"
    ".menu{display:flex;flex-direction:column;gap:8px;}"
    ".menu > summary{list-style:none;cursor:pointer;user-select:none;}"
    ".menu > summary::-webkit-details-marker{display:none;}"
    ".menu-links{display:flex;flex-direction:column;gap:6px;padding-left:12px;margin-top:-2px;}"
    ".menu-link{display:flex;align-items:center;gap:10px;padding:10px 12px;border-radius:12px;color:#5c6673;font-size:13px;font-weight:600;}"
    ".menu-link.active{background:#fff;color:#0d4f99;box-shadow:0 1px 3px rgba(0,0,0,.08);}"
    ".menu-link .bullet{width:6px;height:6px;border-radius:50%;background:#93a4ba;}"
    ".menu-link.active .bullet{background:#0d4f99;}"
    ".sidebar-footer{margin-top:auto;display:flex;flex-direction:column;gap:12px;}"
    ".status-badge{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;background:#dde6f0;border-radius:14px;font-size:11px;font-weight:800;color:#0d4f99;}"
    ".dot{width:8px;height:8px;border-radius:50%;background:#25c26e;animation:pulse 2s infinite;}"
    "@keyframes pulse{0%{transform:scale(1);opacity:1;}50%{transform:scale(.82);opacity:.55;}100%{transform:scale(1);opacity:1;}}"
    ".main{flex:1;min-width:0;}"
    ".topbar{height:72px;background:#fff;border-bottom:1px solid rgba(0,0,0,.06);display:flex;align-items:center;justify-content:space-between;padding:0 24px;position:sticky;top:0;z-index:10;}"
    ".top-left{display:flex;align-items:center;gap:12px;flex-wrap:wrap;}"
    ".title{font-size:20px;font-weight:800;letter-spacing:-.03em;}"
    ".tag{display:inline-flex;align-items:center;padding:4px 8px;background:#eef2f7;border-radius:8px;font-size:10px;font-weight:800;color:#6f7b8e;text-transform:uppercase;letter-spacing:.04em;}"
    ".top-right{display:flex;align-items:center;gap:14px;}"
    ".icons{display:flex;gap:12px;color:#0d4f99;font-size:18px;}"
    ".btn-danger{border:0;background:var(--danger);color:#fff;font-weight:800;font-size:12px;padding:10px 16px;border-radius:12px;cursor:pointer;box-shadow:0 6px 16px rgba(176,0,32,.18);}"
    ".avatar{width:34px;height:34px;border-radius:50%;background:linear-gradient(135deg,#cfd8e3,#8aa0bd);border:2px solid #fff;box-shadow:0 2px 8px rgba(0,0,0,.12);}"
    ".content{padding:26px;max-width:1220px;margin:0 auto;width:100%;}"
    ".grid{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:22px;justify-items:center;}"
    ".card{background:#fff;border-radius:24px;box-shadow:var(--shadow);padding:28px;}"
    ".hero{grid-column:1 / -1;max-width:760px;width:100%;}"
    ".side{grid-column:span 5;display:flex;flex-direction:column;gap:22px;}"
    ".hero-head{display:flex;justify-content:space-between;gap:18px;align-items:flex-start;margin-bottom:28px;}"
    ".hero-head h2{font-size:28px;line-height:1.1;font-weight:900;letter-spacing:-.04em;margin-bottom:8px;}"
    ".hero-head p{font-size:14px;color:var(--muted);max-width:520px;}"
    ".chip{display:inline-flex;align-items:center;justify-content:center;width:48px;height:48px;border-radius:14px;background:#eaf1fb;color:#0d4f99;font-size:20px;flex:0 0 auto;}"
    ".form{display:grid;gap:18px;}"
    ".row{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px;}"
    ".field{display:flex;flex-direction:column;gap:8px;}"
    ".field label{font-size:11px;font-weight:800;text-transform:uppercase;letter-spacing:.08em;color:#6c7684;}"
    ".control{width:100%;border:0;background:#eef2f7;border-bottom:2px solid transparent;border-radius:14px 14px 0 0;padding:14px 16px;font-size:15px;font-weight:600;outline:none;color:#17212d;}"
    ".control:focus{border-bottom-color:var(--primary);background:#e8eef7;}"
    "select.control{appearance:none;background-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 fill=%22none%22 viewBox=%220 0 24 24%22 stroke='%23707a8a'%3E%3Cpath stroke-linecap=%22round%22 stroke-linejoin=%22round%22 stroke-width=%222%22 d=%22M19 9l-7 7-7-7%22/%3E%3C/svg%3E');background-repeat:no-repeat;background-position:right 14px center;background-size:16px;padding-right:42px;}"
    ".submit{margin-top:10px;width:100%;border:0;border-radius:14px;background:linear-gradient(90deg,var(--primary),var(--primary2));color:#fff;font-weight:800;font-size:15px;padding:16px 18px;display:flex;align-items:center;justify-content:center;gap:10px;cursor:pointer;box-shadow:0 10px 22px rgba(11,93,187,.22);}"
    ".diag{background:#f4f7fb;border-radius:24px;padding:22px;box-shadow:var(--shadow);flex:1;display:flex;flex-direction:column;gap:16px;}"
    ".diag h3{font-size:12px;font-weight:900;text-transform:uppercase;letter-spacing:.12em;color:#6c7684;}"
    ".schematic{width:100%;height:220px;border-radius:16px;background:linear-gradient(180deg,#fbfcfe,#eef2f7);border:1px solid var(--border);display:flex;align-items:center;justify-content:center;padding:18px;color:#8a95a4;font-size:14px;font-weight:700;text-align:center;}"
    ".metric{display:flex;flex-direction:column;gap:8px;}"
    ".metric-top{display:flex;justify-content:space-between;gap:12px;font-size:13px;font-weight:600;}"
    ".metric-top .val{color:#0d4f99;font-weight:800;}"
    ".bar{height:7px;background:#d9e2ee;border-radius:999px;overflow:hidden;}"
    ".fill{height:100%;background:#0d4f99;border-radius:inherit;}"
    ".mini{background:#fff;border-radius:24px;padding:20px;display:flex;align-items:center;gap:16px;box-shadow:var(--shadow);}"
    ".mini-icon{width:52px;height:52px;border-radius:50%;background:#eef2f7;display:flex;align-items:center;justify-content:center;color:#0d4f99;font-size:20px;flex:0 0 auto;}"
    ".mini h4{font-size:16px;font-weight:900;margin-bottom:4px;}"
    ".mini p{font-size:13px;color:var(--muted);}"
    ".mini-btn{margin-left:auto;border:0;background:transparent;color:#7b8796;font-size:18px;cursor:pointer;}"
    ".stats{grid-column:span 12;display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:22px;}"
    ".stat{background:#f4f7fb;border-radius:24px;padding:20px;display:flex;align-items:center;gap:14px;}"
    ".stat .icon{width:40px;height:40px;border-radius:50%;background:#fff;display:flex;align-items:center;justify-content:center;flex:0 0 auto;box-shadow:0 2px 8px rgba(0,0,0,.06);}"
    ".label{display:block;font-size:10px;font-weight:900;text-transform:uppercase;letter-spacing:.08em;color:#6c7684;margin-bottom:4px;}"
    ".value{font-size:15px;font-weight:800;}"
    "@media (max-width: 1024px){body{flex-direction:column}.sidebar{width:100%;height:auto;position:static}.hero,.side,.stats{grid-column:span 12}.stats{grid-template-columns:1fr}.row{grid-template-columns:1fr}.content{padding:18px}.topbar{position:static;height:auto;padding:16px 18px;gap:12px;flex-wrap:wrap}}"
    "@media (max-width: 640px){.top-right{width:100%;justify-content:space-between;flex-wrap:wrap}.card,.diag,.mini,.stat{padding:18px}.hero-head h2{font-size:24px}.topbar{padding:14px 16px}.content{padding:14px}}"
    "</style></head><body>"
    "<aside class='sidebar'>"
    "<div class='brand'><h1>HealthSystems</h1><p>Precision Node 04</p></div>"
    "<nav class='nav'>"
    "<a class='nav-link' href='/'><span class='material-symbols-outlined'>home</span><span>Home</span></a>"
    "<a class='nav-link' href='/config.html'><span class='material-symbols-outlined'>hub</span><span>Network</span></a>"
    "<a class='nav-link' href='/modbus_values.html'><span class='material-symbols-outlined' style='font-variation-settings: \'FILL\' 1;'>chat</span><span>Live Readings</span></a>"
    "<details class='menu' open>"
    "<summary class='nav-link active'><span class='material-symbols-outlined'>build</span><span>Maintenance</span></summary>"
    "<div class='menu-links'>"
    "<a class='menu-link active' href='/modbus_config_port.html'><span class='bullet'></span><span>Port</span></a>"
    "<a class='menu-link' href='/modbus_config_slaves.html'><span class='bullet'></span><span>Slaves</span></a>"
    "<a class='menu-link' href='/modbus_config_registers.html'><span class='bullet'></span><span>Registers</span></a>"
    "</div></details>"
    "</nav>"
    "</aside>"
    "<main class='main'>"
    "<header class='topbar'>"
    "<div class='top-left'><span class='title'>UART Port Configuration</span><span class='tag'>Maintenance Mode</span></div>"
    "</header>"
    "<div class='content'><div class='grid'>"
    "<section class='card hero'>"
    "<div class='hero-head'><div><h2>Port Parameters</h2><p>Configure serial communication between Precision Node and external telemetry.</p></div><div class='chip'>⌘</div></div>"
    "<form class='form' method='POST' action='/save_modbus_config' id='configForm'>"
    "<input type='hidden' name='action' value='save_port'>"
    "<div class='row'>"
    "<div class='field'><label for='port_id'>Port Selection</label><select class='control' id='port_id' name='port_id'>"
  );

  for (i = 0; i < MAX_UART_PORTS; i++) {
    http_write(conn, "<option value='");
    http_write_uint(conn, (unsigned long)i);
    http_write(conn, "' ");
    if (current_port == i) {
      http_write(conn, "selected");
    }
    http_write(conn, ">UART_PORT_");
    if ((i + 1u) < 10u) {
      http_write(conn, "0");
    }
    http_write_uint(conn, (unsigned long)(i + 1u));
    http_write(conn, "</option>");
  }

  http_write(conn,
    "</select></div>"
    "<div class='field'><label for='baud'>Baud Rate (bps)</label><select class='control' id='baud' name='baud'>");

  for (baud_index = 0u; baud_index < (uint8_t)(sizeof(baud_rates) / sizeof(baud_rates[0])); baud_index++) {
    http_write(conn, "<option value='");
    http_write_uint(conn, baud_rates[baud_index]);
    http_write(conn, "' ");
    if (appDb.ports[0].baudRate == baud_rates[baud_index]) {
      http_write(conn, "selected");
      current_baud_found = 1u;
    }
    http_write(conn, ">");
    http_write_uint(conn, baud_rates[baud_index]);
    http_write(conn, "</option>");
  }

  if (current_baud_found == 0u) {
    http_write(conn, "<option value='");
    http_write_uint(conn, (unsigned long)appDb.ports[0].baudRate);
    http_write(conn, "' selected>");
    http_write_uint(conn, (unsigned long)appDb.ports[0].baudRate);
    http_write(conn, "</option>");
  }

  http_write(conn,
    "</select></div></div>"
    "<div class='row'>"
    "<div class='field'><label for='stop_bits'>Stop Bits</label><select class='control' id='stop_bits' name='stop_bits'>"
    "<option value='1' ");
  if (appDb.ports[0].stopBits == 1u) {
    http_write(conn, "selected");
  }
  http_write(conn, ">1 bit</option><option value='2' ");
  if (appDb.ports[0].stopBits == 2u) {
    http_write(conn, "selected");
  }
  http_write(conn,
    ">2 bits</option></select></div>"
    "<div class='field'><label for='parity'>Parity Control</label><select class='control' id='parity' name='parity'>"
    "<option value='0' ");
  if (appDb.ports[0].parity == PARITY_NONE) {
    http_write(conn, "selected");
  }
  http_write(conn, ">None</option><option value='1' ");
  if (appDb.ports[0].parity == PARITY_EVEN) {
    http_write(conn, "selected");
  }
  http_write(conn, ">Even</option><option value='2' ");
  if (appDb.ports[0].parity == PARITY_ODD) {
    http_write(conn, "selected");
  }
  http_write(conn,
    ">Odd</option></select></div></div>"
    "<button class='submit' type='submit'><span></span><span>Save Port Configuration</span></button>"
    "</form></section>"
    "</div></div></main></body></html>"
  );
}

/* SLAVES CONFIGURATION PAGE */
void app_config_http_send_slaves_form(struct netconn *conn)
{
  uint8_t i;

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>Slaves Config</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:800px;margin:20px auto}table{border-collapse:collapse;width:100%}td,th{border:1px solid #ccc;padding:6px}form{background:#f9f9f9;padding:15px;margin:15px 0;border-radius:4px}input,select{padding:6px;margin:5px}button{padding:8px 15px;background:#0066cc;color:white;border:none;cursor:pointer;border-radius:4px}button:hover{background:#0052a3}.del-btn{background:#c00}.del-btn:hover{background:#900}</style>"
    "</head><body>"
    "<h1>Configure Modbus Slaves</h1>"
    "<h2>Current Slaves</h2>"
    "<table><tr><th>Name</th><th>Address</th><th>Port</th><th>Registers</th><th>Action</th></tr>"
  );

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      http_write(conn, "<tr><td>");
      http_write_html_escaped(conn, slave_display_name(i));
      http_write(conn, "</td><td>");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);
      http_write(conn, "</td><td>UART");
      http_write_uint(conn, (unsigned long)(appDb.slaveConfig[i].portId + 1u));
      http_write(conn, "</td><td>");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerCount);
      http_write(conn, "</td><td>");
      http_write(conn,
        "<form method='POST' action='/save_modbus_config' style='margin:0;display:inline'>"
        "<input type='hidden' name='action' value='delete_slave'>"
        "<input type='hidden' name='slave_index' value='");
      http_write_uint(conn, (unsigned long)i);
      http_write(conn, "'><button class='del-btn' type='submit'>Delete</button></form></td></tr>");
    }
  }

  http_write(conn, "</table>\n");

  http_write(conn,
    "<h2>Add New Slave</h2>"
    "<form method='POST' action='/save_modbus_config'>"
    "<input type='hidden' name='action' value='add_slave'>"
    "<p><label>Slave Name:<br><input name='slave_name' maxlength='23' style='width:200px' required></label></p>"
    "<p><label>Slave Address (1..247):<br><input name='slave_address' style='width:200px' required></label></p>"
    "<p><label>UART Port:<br><select name='slave_port'>"
  );

  for (i = 0; i < MAX_UART_PORTS; i++) {
    http_write(conn, "<option value='");
    http_write_uint(conn, (unsigned long)i);
    http_write(conn, "'>UART");
    http_write_uint(conn, (unsigned long)(i + 1u));
    http_write(conn, "</option>");
  }

  http_write(conn,
    "</select></label></p>"
    "<p><button type='submit'>Add Slave</button></p>"
    "</form>"
    "<hr><p><strong>Navigation:</strong></p>"
    "<p><a href='/modbus_config_port.html'>Configure Port</a></p>"
    "<p><a href='/modbus_config_registers.html'>Configure Registers</a></p>"
    "<p><a href='/modbus_values.html'>View Values</a></p>"
    "<p><a href='/config.html'>Network Config</a></p>"
    "<p><a href='/'>Home</a></p>"
    "</body></html>"
  );
}

/* REGISTERS CONFIGURATION PAGE */
void app_config_http_send_registers_form(struct netconn *conn)
{
  uint8_t i, j;
  int has_slave = 0;

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>Registers Config</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:800px;margin:20px auto}table{border-collapse:collapse;width:100%}td,th{border:1px solid #ccc;padding:6px}form{background:#f9f9f9;padding:15px;margin:15px 0;border-radius:4px}input,select{padding:6px;margin:5px}button{padding:8px 15px;background:#0066cc;color:white;border:none;cursor:pointer;border-radius:4px}button:hover{background:#0052a3}.del-btn{background:#c00}.del-btn:hover{background:#900}</style>"
    "</head><body>"
    "<h1>Configure Modbus Registers</h1>"
    "<h2>Current Registers</h2>"
    "<table><tr><th>Slave Name</th><th>Slave Addr</th><th>Reg Address</th><th>Type</th><th>Action</th></tr>"
  );

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used == 1u) {
          http_write(conn, "<tr><td>");
          http_write_html_escaped(conn, slave_display_name(i));
          http_write(conn, "</td><td>");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);
          http_write(conn, "</td><td>");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);
          http_write(conn, "</td><td>");
          http_write(conn, register_type_to_text(appDb.slaveConfig[i].registerConfig[j].registerType));
          http_write(conn, "</td><td>");
          http_write(conn,
            "<form method='POST' action='/save_modbus_config' style='margin:0;display:inline'>"
            "<input type='hidden' name='action' value='delete_register'>"
            "<input type='hidden' name='reg_slave_index' value='");
          http_write_uint(conn, (unsigned long)i);
          http_write(conn, "'><input type='hidden' name='reg_address' value='");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);
          http_write(conn, "'><button class='del-btn' type='submit'>Delete</button></form></td></tr>");
        }
      }
    }
  }

  http_write(conn, "</table>\n");

  http_write(conn,
    "<h2>Add New Register</h2>"
    "<form method='POST' action='/save_modbus_config'>"
    "<input type='hidden' name='action' value='add_register'>"
    "<p><label>Slave:<br><select name='reg_slave_index'>"
  );

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      has_slave = 1;
      http_write(conn, "<option value='");
      http_write_uint(conn, (unsigned long)i);
      http_write(conn, "'>");
      http_write_html_escaped(conn, slave_display_name(i));
      http_write(conn, " - addr ");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);
      http_write(conn, "</option>");
    }
  }

  http_write(conn,
    "</select></label></p>"
    "<p><label>Register Address (0-65535):<br><input name='reg_address' type='number' min='0' max='65535' style='width:200px' required></label></p>"
    "<p><label>Register Type:<br><select name='reg_type'>"
    "<option value='0'>U16 (Unsigned 16-bit)</option>"
    "<option value='1'>I16 (Signed 16-bit)</option>"
    "<option value='2'>FLOAT (32-bit float)</option>"
    "</select></label></p>"
    "<p><button type='submit'>Add Register</button></p>"
    "</form>"
  );

  if (has_slave == 0) {
    http_write(conn, "<p style='color:#900;font-weight:bold'>No slaves configured. Add a slave first!</p>");
  }

  http_write(conn,
    "<h2>Alarm thresholds</h2>"
    "<p>Set one upper threshold for each register. If the value goes above it, alarm becomes active.</p>"
    "<form method='POST' action='/save_modbus_config'>"
    "<input type='hidden' name='action' value='update_alarm_thresholds_all'>"
    "<table><tr><th>Slave Name</th><th>Reg Address</th><th>Enabled</th><th>Threshold</th><th>Status</th></tr>"
  );

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used == 1u) {
          http_write(conn, "<tr><td>");
          http_write_html_escaped(conn, slave_display_name(i));
          http_write(conn, "</td><td>");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);
          http_write(conn, "</td><td><select name='alarm_enabled_");
          http_write_uint(conn, (unsigned long)i);
          http_write(conn, "_");
          http_write_uint(conn, (unsigned long)j);
          http_write(conn, "'><option value='0' ");
          if (appDb.slaveConfig[i].registerConfig[j].alarmEnabled == 0u) {
            http_write(conn, "selected");
          }
          http_write(conn, ">Off</option><option value='1' ");
          if (appDb.slaveConfig[i].registerConfig[j].alarmEnabled == 1u) {
            http_write(conn, "selected");
          }
          http_write(conn, ">On</option></select>");
          http_write(conn, "</td><td><input name='alarm_threshold_");
          http_write_uint(conn, (unsigned long)i);
          http_write(conn, "_");
          http_write_uint(conn, (unsigned long)j);
          http_write(conn, "' type='number' step='0.001' value='");
          http_write_float(conn, appDb.slaveConfig[i].registerConfig[j].alarmThreshold);
          http_write(conn, "' style='width:100px'></td><td>");
          http_write(conn, appDb.slaveConfig[i].registerConfig[j].alarmActive ? "ALARM" : (appDb.slaveConfig[i].registerConfig[j].alarmEnabled ? "enabled" : "off"));
          http_write(conn, "</td></tr>");
        }
      }
    }
  }

  http_write(conn, "</table><p><button type='submit'>Save all thresholds</button></p></form>");

  http_write(conn,
    "<hr><p><strong>Navigation:</strong></p>"
    "<p><a href='/modbus_config_port.html'>Configure Port</a></p>"
    "<p><a href='/modbus_config_slaves.html'>Configure Slaves</a></p>"
    "<p><a href='/modbus_values.html'>View Values</a></p>"
    "<p><a href='/config.html'>Network Config</a></p>"
    "<p><a href='/'>Home</a></p>"
    "</body></html>"
  );
}

/* LEGACY: Combined page for backward compatibility */
void app_config_http_send_form(struct netconn *conn)
{
  /* Redirect to port config page */
  http_write(conn,
    "HTTP/1.1 302 Found\r\n"
    "Location: /modbus_config_port.html\r\n"
    "Connection: close\r\n"
    "\r\n"
  );
}



/* Debug page: show lastValue and valid flags for all configured registers */
void app_config_http_send_values(struct netconn *conn)
{
  uint8_t i, j;
  uint8_t alarm_count = 0u;

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if ((appDb.slaveConfig[i].registerConfig[j].used == 1u) &&
            (appDb.slaveConfig[i].registerConfig[j].alarmActive == 1u)) {
          alarm_count++;
        }
      }
    }
  }

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='utf-8'/>"
    "<meta content='width=device-width, initial-scale=1.0' name='viewport'/>"
    "<title>Clinical Precision | Modbus Values</title>"
    "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800;900&display=swap' rel='stylesheet'/>"
    "<link href='https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined:wght,FILL@100..700,0..1&display=swap' rel='stylesheet'/>"
    "<style>"
    ":root{--primary:#00478d;--primary-container:#005eb8;--secondary:#5c5f63;--tertiary:#ba1a1a;--surface:#f7faf9;--surface-container-low:#f1f4f3;--surface-container-lowest:#ffffff;--on-surface:#181c1c;--on-surface-variant:#424752;--outline-variant:#c2c6d4;--error-container:#ffdad6;--on-error-container:#93000a;--text-main:#181c1c;--text-secondary:#64748b;}"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:'Inter',sans-serif;background-color:var(--surface);color:var(--text-main);display:flex;min-height:100vh;overflow:hidden;}"
    "aside{width:256px;background-color:var(--surface-container-low);border-right:1px solid var(--outline-variant);display:flex;flex-direction:column;padding:24px 16px;}"
    ".brand{margin-bottom:40px;padding-left:8px;}"
    ".brand h1{font-size:20px;font-weight:900;color:var(--primary);letter-spacing:-0.025em;}"
    ".brand p{font-size:10px;text-transform:uppercase;letter-spacing:0.1em;color:var(--secondary);font-weight:700;opacity:0.7;}"
    "nav{flex:1;}"
    ".nav-item{display:flex;align-items:center;gap:12px;padding:10px 12px;text-decoration:none;color:#475569;font-size:14px;font-weight:500;border-radius:8px;margin-bottom:4px;transition:background 0.2s;}"
    ".nav-item:hover{background-color:#e2e8f0;}"
    ".nav-item.active{background-color:var(--surface-container-lowest);color:var(--primary);font-weight:600;box-shadow:0 1px 3px rgba(0,0,0,0.05);}"
    ".sidebar-footer{padding-top:16px;border-top:1px solid var(--outline-variant);}"
    ".status-pill{display:flex;align-items:center;gap:12px;padding:8px;}"
    ".status-icon{width:32px;height:32px;border-radius:50%;background-color:var(--primary-container);color:white;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:700;}"
    ".status-text p:first-child{font-size:12px;font-weight:700;}.status-text p:last-child{font-size:10px;color:#16a34a;font-weight:600;}"
    "main{flex:1;display:flex;flex-direction:column;overflow-y:auto;}"
    "header{height:64px;display:flex;justify-content:space-between;align-items:center;padding:0 32px;background-color:var(--surface);position:sticky;top:0;z-index:10;}"
    ".header-left{display:flex;align-items:center;gap:16px;}.header-left h2{font-size:18px;font-weight:700;color:#0f172a;}.divider{width:1px;height:16px;background-color:var(--outline-variant);}.breadcrumb{font-size:14px;font-weight:500;color:var(--text-secondary);}"
    ".header-right{display:flex;align-items:center;gap:24px;}.icon-actions{display:flex;gap:12px;color:#94a3b8;}.icon-btn{background:none;border:none;cursor:pointer;padding:8px;border-radius:50%;transition:background 0.2s;position:relative;}.icon-btn:hover{background-color:#f1f5f9;}.notification-dot{position:absolute;top:8px;right:8px;width:8px;height:8px;background-color:var(--tertiary);border:2px solid var(--surface);border-radius:50%;}.user-profile{display:flex;align-items:center;gap:12px;padding-left:16px;border-left:1px solid var(--outline-variant);}.user-info{text-align:right;}.user-info p:first-child{font-size:12px;font-weight:700;color:#1e293b;}.user-info p:last-child{font-size:10px;color:var(--text-secondary);}.user-avatar{width:36px;height:36px;border-radius:50%;border:2px solid white;box-shadow:0 1px 2px rgba(0,0,0,0.1);}"
    ".content-body{padding:32px;max-width:1200px;}"
    ".alert-banner{background-color:var(--error-container);color:var(--on-error-container);padding:16px;border-radius:12px;border-left:4px solid var(--tertiary);display:flex;justify-content:space-between;align-items:center;margin-bottom:32px;}"
    ".alert-content{display:flex;align-items:center;gap:16px;}.alert-icon{width:40px;height:40px;background-color:var(--tertiary);border-radius:50%;display:flex;align-items:center;justify-content:center;color:white;}.alert-text h3{font-size:14px;font-weight:700;}.alert-text p{font-size:12px;opacity:0.8;}.ack-btn{background:rgba(255,255,255,0.2);border:none;padding:6px 12px;border-radius:4px;font-size:12px;font-weight:700;cursor:pointer;text-transform:uppercase;}"
    ".page-header{display:flex;justify-content:space-between;align-items:flex-end;margin-bottom:24px;gap:16px;}.page-title h2{font-size:30px;font-weight:900;letter-spacing:-0.025em;}.page-title p{font-size:14px;color:var(--text-secondary);}.button-group{display:flex;gap:8px;flex-wrap:wrap;}.btn{padding:8px 16px;border-radius:8px;font-size:12px;font-weight:700;display:flex;align-items:center;gap:8px;cursor:pointer;border:1px solid transparent;transition:opacity 0.2s;}.btn-outline{background:var(--surface-container-lowest);border-color:var(--outline-variant);color:#475569;}.btn-primary{background:var(--primary);color:white;box-shadow:0 4px 6px -1px rgba(0,71,141,0.2);}"
    ".table-container{background:var(--surface-container-lowest);border-radius:16px;border:1px solid rgba(0,0,0,0.05);overflow:hidden;box-shadow:0 4px 12px rgba(0,71,141,0.04);margin-bottom:32px;}table{width:100%;border-collapse:collapse;}thead{background:var(--surface-container-low);}th{padding:16px 24px;text-align:left;font-size:10px;text-transform:uppercase;letter-spacing:0.1em;font-weight:900;color:#64748b;}td{padding:16px 24px;border-bottom:1px solid #f1f5f9;font-size:14px;vertical-align:middle;}.slave-name{font-weight:700;color:#1e293b;}.val-unit{font-size:10px;color:#94a3b8;font-weight:700;margin-left:4px;}.val-large{font-size:18px;font-weight:900;}.type-tag{background:#f1f5f9;font-family:monospace;padding:2px 8px;border-radius:4px;font-size:12px;}.status-alarm{background:var(--tertiary);color:white;padding:4px 8px;border-radius:4px;font-size:10px;font-weight:900;display:inline-flex;align-items:center;gap:6px;}.pulse{width:6px;height:6px;background:white;border-radius:50%;animation:pulse-anim 1s infinite;}@keyframes pulse-anim{0%{opacity:1;}50%{opacity:0.4;}100%{opacity:1;}}.status-off{color:#94a3b8;font-weight:700;text-transform:uppercase;font-size:10px;}"
    ".bento-grid{display:grid;grid-template-columns:repeat(12, 1fr);gap:24px;margin-top:32px;}.bento-large{grid-column:span 8;background:var(--surface-container-low);padding:24px;border-radius:16px;position:relative;min-height:180px;}.bento-small{grid-column:span 4;background:var(--primary);color:white;padding:24px;border-radius:16px;display:flex;flex-direction:column;justify-content:space-between;}.bento-label{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:0.1em;color:var(--primary);margin-bottom:8px;}.bento-small .bento-label{color:#a9c7ff;}.bento-title{font-size:24px;font-weight:900;color:#1e293b;max-width:400px;}.progress-bar{height:4px;background:#e2e8f0;border-radius:2px;margin-top:24px;width:100%;position:relative;}.progress-fill{height:100%;background:var(--primary);border-radius:2px;width:84%;}"
    "footer{margin-top:32px;padding-top:24px;border-top:1px solid var(--outline-variant);display:flex;justify-content:space-between;align-items:center;gap:16px;flex-wrap:wrap;}.back-link{text-decoration:none;color:var(--primary);font-size:14px;font-weight:700;display:flex;align-items:center;gap:8px;}.session-info{font-size:10px;color:#94a3b8;font-weight:500;}.material-symbols-outlined{font-size:20px;vertical-align:middle;}"
    "@media (max-width: 1024px){body{flex-direction:column;overflow:auto;}aside{width:100%;border-right:none;border-bottom:1px solid var(--outline-variant);}main{overflow:visible;}header{height:auto;padding:16px 20px;gap:12px;flex-wrap:wrap;}.content-body{padding:20px;}.bento-large,.bento-small{grid-column:span 12;}.page-header{align-items:flex-start;flex-direction:column;}.header-right{width:100%;justify-content:space-between;flex-wrap:wrap;}.user-profile{padding-left:0;border-left:none;}}"
    "@media (max-width: 640px){header{padding:14px 16px;}.content-body{padding:16px;}.page-title h2{font-size:24px;}.button-group{width:100%;}.btn{justify-content:center;flex:1 1 140px;}.table-container{overflow-x:auto;}th,td{padding:12px 14px;white-space:nowrap;}}"
    "</style></head><body>"
    "<aside>"
    "<div class='brand'><h1>HealthSystems</h1><p>Precision Node 04</p></div>"
    "<nav>"
    "<a class='nav-item' href='/'><span class='material-symbols-outlined'>home</span>Home</a>"
    "<a class='nav-item' href='/config.html'><span class='material-symbols-outlined'>hub</span>Network</a>"
    "<a class='nav-item active' href='/modbus_values.html'><span class='material-symbols-outlined' style='font-variation-settings: \'FILL\' 1;'>chat</span>Live Readings</a>"
    "<a class='nav-item' href='/modbus_config_port.html'><span class='material-symbols-outlined'>build</span>Maintenance</a>"
    "</nav>"
    "<div class='sidebar-footer'><div class='status-pill'><div class='status-icon'>SN04</div><div class='status-text'><p>System Status</p><p>Nominal</p></div></div></div>"
    "</aside>"
    "<main>"
    "<header>"
    "<div class='header-left'><h2>Clinical Precision Home</h2><div class='divider'></div><span class='breadcrumb'>Node / Modbus Values</span></div>"
    "<div class='header-right'><div class='icon-actions'><button class='icon-btn' type='button' id='ackBtn'><span class='material-symbols-outlined'>notifications</span><span class='notification-dot'></span></button><button class='icon-btn' type='button' onclick='fetchValues()'><span class='material-symbols-outlined'>refresh</span></button></div></div>"
    "</header>"
    "<div class='content-body'>"
  );

  if (alarm_count > 0u) {
    http_write(conn, "<div id='alarmBanner' class='alert-banner'><div class='alert-content'><div class='alert-icon'><span class='material-symbols-outlined' style='font-variation-settings: \'FILL\' 1;'>warning</span></div><div class='alert-text'><h3>System Alert Triggered</h3><p>");
    http_write_uint(conn, (unsigned long)alarm_count);
    http_write(conn, " alarm(s) active - Action required on Node 04 Communication Layer.</p></div></div><button class='ack-btn' type='button'>Acknowledge</button></div>");
  } else {
    http_write(conn, "<div id='alarmBanner' class='alert-banner' style='background:var(--surface-container-lowest);color:var(--on-surface);border-left-color:#16a34a;'><div class='alert-content'><div class='alert-icon' style='background:#16a34a;'><span class='material-symbols-outlined' style='font-variation-settings: \'FILL\' 1;'>check_circle</span></div><div class='alert-text'><h3>No active alarms</h3><p>All monitored registers are within threshold.</p></div></div><button class='ack-btn' type='button'>Refresh</button></div>");
  }

  http_write(conn,
    "<div class='page-header'><div class='page-title'><h2>Modbus Values</h2><p>Real-time telemetry stream from peripheral medical sensors.</p></div></div>"
    "<div class='table-container'><table id='valuesTable'><thead><tr><th>Slave Name</th><th>Address</th><th>Register</th><th>Type</th><th>Value</th><th>Threshold</th><th>Status</th><th>Valid</th></tr></thead><tbody id='valuesBody'>"
  );

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        registerConfig_t *reg;

        reg = &appDb.slaveConfig[i].registerConfig[j];
        if (reg->used == 1u) {
          http_write(conn, "<tr class='");
          http_write(conn, reg->alarmActive ? "alarm" : (reg->alarmEnabled ? "enabled" : "ok"));
          http_write(conn, "'><td>");
          http_write_html_escaped(conn, slave_display_name(i));
          http_write(conn, "</td><td>");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);
          http_write(conn, "</td><td>");
          http_write_uint(conn, (unsigned long)reg->regAddress);
          http_write(conn, "</td><td>");
          http_write(conn, register_type_to_text(reg->registerType));
          http_write(conn, "</td><td>");
          if (reg->valid == 1u) {
            http_write_float(conn, reg->lastValue);
          } else {
            http_write(conn, "n/a");
          }
          http_write(conn, "</td><td>");
          if (reg->alarmEnabled == 1u) {
            http_write_float(conn, reg->alarmThreshold);
          } else {
            http_write(conn, "off");
          }
          http_write(conn, "</td><td>");
          http_write(conn, reg->alarmActive ? "ALARM" : (reg->alarmEnabled ? "enabled" : "off"));
          http_write(conn, "</td><td>");
          http_write(conn, reg->valid ? "yes" : "no");
          http_write(conn, "</td></tr>");
        }
      }
    }
  }

  http_write(conn, "</tbody></table></div>");



  http_write(conn, "<footer><a class='back-link' href='/modbus_config_port.html'><span class='material-symbols-outlined'>arrow_back</span>Back to Modbus configuration</a><p class='session-info'>Live page updated from /api/modbus_values</p></footer>");

  /* small polling script: fetch JSON every second and update table/banner (DOM-based, safer escaping) */
  http_write(conn, "<script>\n");
  http_write(conn, "function renderValues(data){\n");
  http_write(conn, " var tbody = document.getElementById('valuesBody');\n");
  http_write(conn, " tbody.innerHTML = '';\n");
  http_write(conn, " data.registers.forEach(function(r){\n");
  http_write(conn, "  var tr = document.createElement('tr');\n");
  http_write(conn, "  tr.className = r.alarmActive ? 'alarm' : (r.alarmEnabled ? 'enabled' : 'ok');\n");
  http_write(conn, "  function td(v){ var d = document.createElement('td'); d.textContent = v; return d; }\n");
  http_write(conn, "  var nameCell = td(r.slaveName || ('Slave ' + (r.slaveIndex + 1))); nameCell.className = 'slave-name'; tr.appendChild(nameCell);\n");
  http_write(conn, "  tr.appendChild(td(r.slaveAddress));\n");
  http_write(conn, "  tr.appendChild(td(r.regAddress));\n");
  http_write(conn, "  var typeCell = td(''); var typeTag = document.createElement('span'); typeTag.className = 'type-tag'; typeTag.textContent = r.type; typeCell.appendChild(typeTag); tr.appendChild(typeCell);\n");
  http_write(conn, "  var valueCell = td(''); var valueWrap = document.createElement('span'); valueWrap.className = 'val-large'; valueWrap.textContent = r.valid ? r.lastValue : 'n/a'; valueCell.appendChild(valueWrap); tr.appendChild(valueCell);\n");
  http_write(conn, "  var thresholdCell = td(''); if (r.alarmEnabled) { var thresholdWrap = document.createElement('span'); thresholdWrap.className = 'val-large'; thresholdWrap.textContent = r.alarmThreshold; thresholdCell.appendChild(thresholdWrap); } else { var off = document.createElement('span'); off.className = 'status-off'; off.textContent = 'off'; thresholdCell.appendChild(off); } tr.appendChild(thresholdCell);\n");
  http_write(conn, "  var statusCell = td(''); if (r.alarmActive) { var s = document.createElement('span'); s.className = 'status-alarm'; var p = document.createElement('span'); p.className = 'pulse'; s.appendChild(p); s.appendChild(document.createTextNode('ALARM')); statusCell.appendChild(s); } else { var off2 = document.createElement('span'); off2.className = 'status-off'; off2.textContent = r.alarmEnabled ? 'enabled' : 'off'; statusCell.appendChild(off2); } tr.appendChild(statusCell);\n");
  http_write(conn, "  var validCell = td(''); var icon = document.createElement('span'); icon.className = 'material-symbols-outlined'; icon.style.fontVariationSettings = '\\'FILL\\' 1'; icon.style.color = r.valid ? '#16a34a' : '#94a3b8'; icon.textContent = r.valid ? 'check_circle' : 'cancel'; validCell.appendChild(icon); tr.appendChild(validCell);\n");
  http_write(conn, "  tbody.appendChild(tr);\n");
  http_write(conn, " });\n");
  http_write(conn, " var b = document.getElementById('alarmBanner');\n");
  http_write(conn, " if (data.alarm_count > 0){ b.className='alert-banner'; b.style.background='var(--error-container)'; b.style.color='var(--on-error-container)'; b.style.borderLeftColor='var(--tertiary)'; b.innerHTML = '<div class=\\'alert-content\\'><div class=\\'alert-icon\\'><span class=\\'material-symbols-outlined\\' style=\\'font-variation-settings: \\\'FILL\\' 1;\\'>warning</span></div><div class=\\'alert-text\\'><h3>System Alert Triggered</h3><p>' + data.alarm_count + ' alarm(s) active - Action required on Node 04 Communication Layer.</p></div></div><button class=\\'ack-btn\\' type=\\'button\\'>Acknowledge</button>'; } else { b.className='alert-banner'; b.style.background='var(--surface-container-lowest)'; b.style.color='var(--on-surface)'; b.style.borderLeftColor='#16a34a'; b.innerHTML = '<div class=\\'alert-content\\'><div class=\\'alert-icon\\' style=\\'background:#16a34a;\\'><span class=\\'material-symbols-outlined\\' style=\\'font-variation-settings: \\\'FILL\\' 1;\\'>check_circle</span></div><div class=\\'alert-text\\'><h3>No active alarms</h3><p>All monitored registers are within threshold.</p></div></div><button class=\\'ack-btn\\' type=\\'button\\'>Refresh</button>'; }\n");
  http_write(conn, "}\n");
  http_write(conn, "function fetchValues(){ fetch('/api/modbus_values').then(function(r){ return r.json(); }).then(function(j){ renderValues(j); }).catch(function(){}); }\n");
  http_write(conn, "document.getElementById('ackBtn').addEventListener('click', fetchValues);\n");
  http_write(conn, "setInterval(fetchValues,1000);fetchValues();\n");
  http_write(conn, "</script>");
  http_write(conn, "</div></main></body></html>");
}

/* JSON endpoint used by the values page to refresh without reloading. */
void app_config_http_send_api_values(struct netconn *conn)
{
  uint8_t i, j;
  int first = 1;
  uint8_t alarm_count = 0u;

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if ((appDb.slaveConfig[i].registerConfig[j].used == 1u) &&
            (appDb.slaveConfig[i].registerConfig[j].alarmActive == 1u)) {
          alarm_count++;
        }
      }
    }
  }

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-cache, no-store, must-revalidate\r\n"
    "Pragma: no-cache\r\n"
    "Expires: 0\r\n"
    "Connection: close\r\n\r\n"
  );

  /* Write JSON object start including alarm_count */
  http_write(conn, "{\"alarm_count\":");
  http_write_uint(conn, (unsigned long)alarm_count);
  http_write(conn, ",\"registers\":[");

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used == 1u) {
          if (!first) {
            http_write(conn, ",");
          }
          first = 0;
          http_write(conn, "{\"slaveIndex\":");
          http_write_uint(conn, (unsigned long)i);

          http_write(conn, ",\"slaveName\":\"");
          http_write_json_escaped(conn, slave_display_name(i));
          http_write(conn, "\"");

          http_write(conn, ",\"slaveAddress\":");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);

          http_write(conn, ",\"regAddress\":");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);

          http_write(conn, ",\"type\":\"");
          http_write(conn, register_type_to_text(appDb.slaveConfig[i].registerConfig[j].registerType));
          http_write(conn, "\"");

          http_write(conn, ",\"lastValue\":");
          http_write_float(conn, appDb.slaveConfig[i].registerConfig[j].lastValue);

          http_write(conn, ",\"valid\":");
          http_write(conn, appDb.slaveConfig[i].registerConfig[j].valid ? "1" : "0");

          http_write(conn, ",\"alarmEnabled\":");
          http_write(conn, appDb.slaveConfig[i].registerConfig[j].alarmEnabled ? "1" : "0");

          http_write(conn, ",\"alarmThreshold\":");
          http_write_float(conn, appDb.slaveConfig[i].registerConfig[j].alarmThreshold);

          http_write(conn, ",\"alarmActive\":");
          http_write(conn, appDb.slaveConfig[i].registerConfig[j].alarmActive ? "1" : "0");

          http_write(conn, "}");
        }
      }
    }
  }

  /* Prepend alarm_count at top-level for quick banner */
  /* We sent registers array already; to include alarm_count we rebuild small wrapper. Simpler: send as object with alarm_count then registers. */
  /* Current output is '{"registers":[...'] so we need to reformat - easier to send properly from scratch. */
  /* For simplicity, close registers array then prepend alarm_count by rewriting output: send closing and then a small wrapper. */
  http_write(conn, "]}\n");
}

/*
 * app_config_http_send_api_slaves(conn)
 * 
 * OBJECTIF: Envoyer la liste des esclaves configurés en JSON
 * PARAMÈTRES:
 *  - conn: connexion netconn (socket TCP) vers le client
 * 
 * FORMAT JSON: [{"index":0,"address":10,"port":0,"registers":5}, ...]
 */
void app_config_http_send_api_slaves(struct netconn *conn)
{
  uint8_t i;
  int first = 1;

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n\r\n"
    "["
  );

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      if (!first) {
        http_write(conn, ",");
      }
      first = 0;

      http_write(conn, "{\"index\":");
      http_write_uint(conn, (unsigned long)i);

      http_write(conn, ",\"name\":\"");
      http_write_json_escaped(conn, slave_display_name(i));
      http_write(conn, "\"");

      http_write(conn, ",\"address\":");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);

      http_write(conn, ",\"port\":");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].portId);

      http_write(conn, ",\"registers\":");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerCount);

      http_write(conn, "}");
    }
  }

  http_write(conn, "]");
}

/*
 * app_config_http_send_api_registers(conn)
 * 
 * OBJECTIF: Envoyer la liste des registres configurés en JSON
 * PARAMÈTRES:
 *  - conn: connexion netconn (socket TCP) vers le client
 * 
 * FORMAT JSON: [{"slave_index":0,"slave_address":10,"reg_address":100,"type":"U16"}, ...]
 */
void app_config_http_send_api_registers(struct netconn *conn)
{
  uint8_t i, j;
  int first = 1;

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n\r\n"
    "["
  );

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used == 1u) {
          if (!first) {
            http_write(conn, ",");
          }
          first = 0;

          http_write(conn, "{\"slave_index\":");
          http_write_uint(conn, (unsigned long)i);

          http_write(conn, ",\"slave_name\":\"");
          http_write_json_escaped(conn, slave_display_name(i));
          http_write(conn, "\"");

          http_write(conn, ",\"slave_address\":");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);

          http_write(conn, ",\"reg_address\":");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);

          http_write(conn, ",\"type\":\"");
          http_write(conn, register_type_to_text(appDb.slaveConfig[i].registerConfig[j].registerType));
          http_write(conn, "\"}");
        }
      }
    }
  }

  http_write(conn, "]");
}

/*
 * app_config_http_send_api_modbus_config(conn)
 * 
 * OBJECTIF: Envoyer slaves ET registers en UNE SEULE réponse JSON
 * 
 * AVANTAGE CRUCIAL: 
 *  - Une seule requête HTTP au lieu de 3
 *  - Une seule connexion TCP au lieu de 3
 *  - Pas de blocage parallèle sur serveur single-threaded LwIP
 *  - Sur STM32 embedded, cela fait une énorme différence de latence
 * 
 * FORMAT JSON:
 *  {
 *    "slaves": [{"index":0,"address":10,"port":0,"registers":5}, ...],
 *    "registers": [{"slave_index":0,"slave_address":10,"reg_address":100,"type":"U16"}, ...]
 *  }
 */
void app_config_http_send_api_modbus_config(struct netconn *conn)
{
  uint8_t i, j;
  int first_slave = 1;
  int first_reg = 1;

  http_write(conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n\r\n"
    "{\"slaves\":["
  );

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      if (!first_slave) {
        http_write(conn, ",");
      }
      first_slave = 0;

      http_write(conn, "{\"index\":");
      http_write_uint(conn, (unsigned long)i);

      http_write(conn, ",\"address\":");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);

      http_write(conn, ",\"port\":");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].portId);

      http_write(conn, ",\"registers\":");
      http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerCount);

      http_write(conn, "}");
    }
  }

  http_write(conn, "],\"registers\":[");

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used == 1u) {
          if (!first_reg) {
            http_write(conn, ",");
          }
          first_reg = 0;

          http_write(conn, "{\"slave_index\":");
          http_write_uint(conn, (unsigned long)i);

          http_write(conn, ",\"slave_name\":\"");
          http_write_json_escaped(conn, slave_display_name(i));
          http_write(conn, "\"");

          http_write(conn, ",\"slave_address\":");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].slaveAddress);

          http_write(conn, ",\"reg_address\":");
          http_write_uint(conn, (unsigned long)appDb.slaveConfig[i].registerConfig[j].regAddress);

          http_write(conn, ",\"type\":\"");
          http_write(conn, register_type_to_text(appDb.slaveConfig[i].registerConfig[j].registerType));
          http_write(conn, "\"}");
        }
      }
    }
  }

  http_write(conn, "]}");
}

/*
 * app_config_http_handle_save(conn, request, request_len)
 * 
 * OBJECTIF: Traiter TOUS les POSTs (actions) de la page de configuration Modbus
 * 
 * PARAMÈTRES:
 *  - conn: connexion netconn (socket TCP) vers le client\n *  - request: pointeur vers le corps HTTP POST complet (headers + body)\n *  - request_len: longueur totale du request (non utilisée, on cherche les délimiteurs)
 * 
 * ACTIONS SUPPORTÉES:\n * \n *  1. action=save_port\n *     Données du formulaire: baud, stop_bits, parity, port_id\n *     Comportement:\n *       a. Extrait et parse baud (0..2000000), stop_bits (1 ou 2), parity (0,1,2)\n *       b. Valide que port_id < MAX_UART_PORTS\n *       c. Appelle appConfig_updatePort() pour appliquer la config UART:\n *          - Prend mutex\n *          - Snapshot RAM et hardware\n *          - Appelle rs485_interface_apply_config() pour changer hardware (HAL)\n *          - Sur succès: appelle modbusMaster_onUartReconfig() et persiste\n *          - Sur erreur: restaure avec rs485_interface_restore_config()\n *       d. Envoie page de statut (succès ou erreur)\n *\n *  2. action=add_slave\n *     Données du formulaire: slave_address (1..247), slave_port (UART index)\n *     Comportement:\n *       a. Extrait et parse slave_address et slave_port\n *       b. Valide ranges (address 1-247, port < MAX_UART_PORTS)\n *       c. Appelle appConfig_addSlave() pour ajouter esclave à la DB RAM\n *       d. Appelle appConfig_save() pour persister en Flash\n *       e. Envoie page de statut\n *\n *  3. action=delete_slave\n *     Données du formulaire: slave_index (index dans appDb.slaveConfig[])\n *     Comportement:\n *       a. Extrait et parse slave_index\n *       b. Appelle appConfig_removeSlave() qui:\n *          - Marque l'esclave comme unused\n *          - NETTOIE AUSSI tous ses registres (cascade cleanup) ==> TRÈS IMPORTANT\n *       c. Appelle appConfig_save() pour persister\n *       d. Envoie page de statut\n *\n *  4. action=add_register\n *     Données du formulaire: reg_slave_index, reg_address (0..65535), reg_type (0=U16, 1=I16, 2=FLOAT)\n *     Comportement:\n *       a. Extrait et parse tous les champs\n *       b. Valide ranges et que l'esclave existe et n'est pas plein\n *       c. Appelle appConfig_addRegister() pour ajouter registre à la DB\n *       d. Appelle appConfig_save() pour persister\n *       e. Envoie page de statut\n *\n *  5. action=delete_register\n *     Données du formulaire: reg_slave_index, reg_address\n *     Comportement:\n *       a. Extrait et parse les deux champs\n *       b. Appelle appConfig_removeRegister() pour supprimer le registre\n *       c. Appelle appConfig_save() pour persister\n *       d. Envoie page de statut\n *\n * FLUX HTTP:\n *  1. Cherche le corps HTTP après \"\\r\\n\\r\\n\" (délimiteur headers/body)\n *  2. Appelle get_param() pour extraire les champs du formulaire\n *  3. Parse les valeurs numériques avec parse_ulong()\n *  4. Valide les champs avec des checks simples (range, format)\n *  5. Appelle la fonction app_config_* appropriée\n *  6. Envoie une page de statut (200 OK + message si succès, 400 Bad Request + message si erreur)\n *\n * POURQUOI:\n *  - Centre TOUTES les actions POST au même endroit => Facile à maintenir\n *  - Chaque action est validée (types, ranges) => Évite les données invalides en Flash\n *  - Sauvegarde en Flash après chaque action => Persistance immédiate\n *  - Application hardware (save_port) via appConfig_updatePort() => Changements réels sur MCU\n *  - Cascade cleanup (delete_slave nettoie registres) => Pas de données orphelines\n *\n * SANS CETTE FONCTION:\n *  - Les POSTs du formulaire ne seraient pas traités => Rien ne changerait\n *  - Impossible de configurer Modbus depuis le web => PRODUIT INUTILE\n */
void app_config_http_handle_save(struct netconn *conn, const char *request, unsigned short request_len)
{
  const char *body;
  char action[32];
  unsigned long n1;
  unsigned long n2;
  unsigned long n3;

  (void)request_len;

  if (request == NULL) {
    send_status_page(conn, 0, "Request is empty");
    return;
  }

  body = strstr(request, "\r\n\r\n");
  if (body == NULL) {
    send_status_page(conn, 0, "HTTP body is missing");
    return;
  }
  body += 4;

  if (!get_param(body, "action", action, sizeof(action))) {
    send_status_page(conn, 0, "Missing action field");
    return;
  }

  if (strcmp(action, "save_port") == 0) {
    handle_save_port(conn, body);
    return;
  }

  if (strcmp(action, "add_slave") == 0) {
    char v_name[MAX_SLAVE_NAME_LEN];
    char v_addr[8];
    char v_port[8];

    if ((!get_param(body, "slave_address", v_addr, sizeof(v_addr))) ||
        (!get_param(body, "slave_name", v_name, sizeof(v_name))) ||
        (!get_param(body, "slave_port", v_port, sizeof(v_port)))) {
      send_status_page(conn, 0, "Slave fields are missing");
      return;
    }

    if (v_name[0] == '\0') {
      send_status_page(conn, 0, "Slave name is required");
      return;
    }

    if ((!parse_ulong(v_addr, &n1)) || (!parse_ulong(v_port, &n2))) {
      send_status_page(conn, 0, "Slave fields are invalid numbers");
      return;
    }

    if ((n1 == 0ul) || (n1 > 247ul) || (n2 >= (unsigned long)MAX_UART_PORTS)) {
      send_status_page(conn, 0, "Slave fields are out of range");
      return;
    }

    if (appConfig_addSlave((uint8_t)n1, (uartPortId_t)n2, v_name) < 0) {
      send_status_page(conn, 0, "Cannot add slave (duplicate address, duplicate name, or full table)");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after slave add");
      return;
    }

    send_status_page(conn, 1, "Slave added and saved");
    return;
  }

  if (strcmp(action, "delete_slave") == 0) {
    char v_idx[8];

    if (!get_param(body, "slave_index", v_idx, sizeof(v_idx))) {
      send_status_page(conn, 0, "Missing slave index");
      return;
    }

    if (!parse_ulong(v_idx, &n1)) {
      send_status_page(conn, 0, "Slave index is invalid");
      return;
    }

    if (appConfig_removeSlave((uint8_t)n1) < 0) {
      send_status_page(conn, 0, "Cannot remove slave");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after slave delete");
      return;
    }

    send_status_page(conn, 1, "Slave deleted with cascade register cleanup");
    return;
  }

  if (strcmp(action, "add_register") == 0) {
    char v_slave[8];
    char v_addr[10];
    char v_type[8];

    if ((!get_param(body, "reg_slave_index", v_slave, sizeof(v_slave))) ||
        (!get_param(body, "reg_address", v_addr, sizeof(v_addr))) ||
        (!get_param(body, "reg_type", v_type, sizeof(v_type)))) {
      send_status_page(conn, 0, "Register fields are missing");
      return;
    }

    if ((!parse_ulong(v_slave, &n1)) || (!parse_ulong(v_addr, &n2)) || (!parse_ulong(v_type, &n3))) {
      send_status_page(conn, 0, "Register fields are invalid numbers");
      return;
    }

    if ((n1 >= (unsigned long)MAX_SLAVES) || (n2 > 65535ul) || (n3 > (unsigned long)REG_TYPE_FLOAT)) {
      send_status_page(conn, 0, "Register fields are out of range");
      return;
    }

    if (appConfig_addRegister((uint8_t)n1, (uint16_t)n2, (registerType_t)n3) < 0) {
      send_status_page(conn, 0, "Cannot add register (duplicate, invalid slave, or full table)");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after register add");
      return;
    }

    send_status_page(conn, 1, "Register added and saved");

    return;
  }

  if (strcmp(action, "update_alarm_thresholds") == 0) {
    char v_slave[8];
    char v_addr[10];
    char v_enabled[8];
    char v_threshold[24];
    float alarm_threshold;
    registerConfig_t *registerConfig;

    if ((!get_param(body, "alarm_slave_index", v_slave, sizeof(v_slave))) ||
        (!get_param(body, "alarm_reg_address", v_addr, sizeof(v_addr))) ||
        (!get_param(body, "alarm_enabled", v_enabled, sizeof(v_enabled))) ||
        (!get_param(body, "alarm_threshold", v_threshold, sizeof(v_threshold)))) {
      send_status_page(conn, 0, "Alarm fields are missing");
      return;
    }

    if ((!parse_ulong(v_slave, &n1)) || (!parse_ulong(v_addr, &n2)) || (!parse_ulong(v_enabled, &n3)) ||
        (!parse_float_value(v_threshold, &alarm_threshold))) {
      send_status_page(conn, 0, "Alarm fields are invalid numbers");
      return;
    }

    if ((n1 >= (unsigned long)MAX_SLAVES) || (n3 > 1ul) || (alarm_threshold < 0.0f)) {
      send_status_page(conn, 0, "Alarm values are out of range");
      return;
    }

    registerConfig = find_register_config((uint8_t)n1, (uint16_t)n2);
    if (registerConfig == 0) {
      send_status_page(conn, 0, "Register not found");
      return;
    }

    registerConfig->alarmEnabled = (uint8_t)n3;
    registerConfig->alarmThreshold = alarm_threshold;
    registerConfig->alarmActive = 0u;

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after alarm update");
      return;
    }

    send_status_page(conn, 1, "Alarm threshold updated and saved");
    return;
  }

  if (strcmp(action, "update_alarm_thresholds_all") == 0) {
    uint8_t i;
    uint8_t j;
    uint8_t updated_count = 0u;
    char v_enabled[8];
    char v_threshold[24];
    float alarm_threshold;
    registerConfig_t *registerConfig;

    for (i = 0; i < MAX_SLAVES; i++) {
      if (appDb.slaveConfig[i].used == 1u) {
        for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
          registerConfig = &appDb.slaveConfig[i].registerConfig[j];
          if (registerConfig->used == 1u) {
            if ((!get_indexed_param(body, "alarm_enabled", (unsigned long)i, (unsigned long)j, v_enabled, sizeof(v_enabled))) ||
                (!get_indexed_param(body, "alarm_threshold", (unsigned long)i, (unsigned long)j, v_threshold, sizeof(v_threshold))) ||
                (!parse_ulong(v_enabled, &n1)) || (!parse_float_value(v_threshold, &alarm_threshold))) {
              send_status_page(conn, 0, "One or more threshold fields are invalid");
              return;
            }

            if ((n1 > 1ul) || (alarm_threshold < 0.0f)) {
              send_status_page(conn, 0, "One or more threshold values are out of range");
              return;
            }

            registerConfig->alarmEnabled = (uint8_t)n1;
            registerConfig->alarmThreshold = alarm_threshold;
            registerConfig->alarmActive = 0u;
            updated_count++;
          }
        }
      }
    }

    if (updated_count == 0u) {
      send_status_page(conn, 0, "No thresholds to update");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after threshold update");
      return;
    }

    send_status_page(conn, 1, "All thresholds updated and saved");
    return;
  }

  if (strcmp(action, "delete_register") == 0) {
    char v_slave[8];
    char v_addr[10];

    if ((!get_param(body, "reg_slave_index", v_slave, sizeof(v_slave))) ||
        (!get_param(body, "reg_address", v_addr, sizeof(v_addr)))) {
      send_status_page(conn, 0, "Missing register delete fields");
      return;
    }

    if ((!parse_ulong(v_slave, &n1)) || (!parse_ulong(v_addr, &n2))) {
      send_status_page(conn, 0, "Register delete fields are invalid numbers");
      return;
    }

    if (appConfig_removeRegister((uint8_t)n1, (uint16_t)n2) < 0) {
      send_status_page(conn, 0, "Cannot remove register");
      return;
    }

    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after register delete");
      return;
    }

    send_status_page(conn, 1, "Register deleted and saved");
    return;
  }

  send_status_page(conn, 0, "Unknown action");
}
