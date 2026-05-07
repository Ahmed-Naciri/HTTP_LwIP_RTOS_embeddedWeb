#include "app_config_http.h"

#include "app_config.h"
#include "main.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 *  g_app_config_page_buffer: buffer de 12000 octets pour stocker la page HTML générée
 *  g_page_used: nombre d'octets actuellement utilisés dans le buffer
 * 
 * Ces variables évitent les allocations dynamiques (heap) qui sont risquées en embedded.
 * Sans elles, on devrait allouer/libérer mémoire, ce qui peut causer de la fragmentation.
 */
static char g_app_config_page_buffer[12000];
static size_t g_page_used = 0u;

/*
 * hex_to_nibble(c)
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
 * page_appendf(fmt, ...)
 * 
 * OBJECTIF: Ajouter du texte formaté (comme printf) au buffer HTML
 * 
 * MÉCANISME:
 *  - Prend un format string et des arguments variables
 *  - Utilise vsnprintf() pour formater dans un buffer temporaire local (320 octets)
 *  - Appelle page_append() pour ajouter le résultat au buffer HTML
 * 
 * POURQUOI: On doit générer du HTML avec des valeurs dynamiques (addresses esclaves, baud rates).
 *           page_appendf permet d'utiliser la syntaxe printf familierère pour construire le HTML.
 * 
 * UTILISATION: Appelée rarement dans ce fichier (la plupart utilise page_append direct).
 *              Utile pour les étiquettes avec texte varié.
 * 
 * SANS CETTE FONCTION: Impossible de construire du HTML dynamique avec valeurs
 *                      => Faudrait tout faire avec du texte statique => inflexible.
 */
static void page_appendf(const char *fmt, ...)
{
  char line[320];
  va_list args;

  va_start(args, fmt);
  (void)vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);

  page_append(line);
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
  page_append(ok ? "<h1>Operation completed</h1>" : "<h1>Operation failed</h1>");
  page_append("<p>");
  page_append((message != NULL) ? message : "No details");
  page_append("</p>");
  page_append("<p><a href=\"/modbus_config.html\">Back to Modbus configuration</a></p>");
  page_append("</body></html>");

  netconn_write(conn, g_app_config_page_buffer, g_page_used, NETCONN_COPY);
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
              "<tr><th>Index</th><th>Address</th><th>Port</th><th>Registers</th><th>Action</th></tr>");
  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      page_append("<tr>");
      page_append("<td>");
      page_append_uint((unsigned long)i);
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
              "<tr><th>Slave index</th><th>Slave address</th><th>Register address</th><th>Type</th><th>Action</th></tr>");

  for (i = 0; i < MAX_SLAVES; i++) {
    if (appDb.slaveConfig[i].used == 1u) {
      for (j = 0; j < MAX_REGISTERS_PER_SLAVE; j++) {
        if (appDb.slaveConfig[i].registerConfig[j].used == 1u) {
          page_append("<tr>");
          page_append("<td>");
          page_append_uint((unsigned long)i);
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
      page_append("\">idx ");
      page_append_uint((unsigned long)i);
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
void app_config_http_send_form(struct netconn *conn)
{
  page_reset();
  page_append("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
  page_append("<!doctype html><html><head><meta charset=\"utf-8\"><title>Modbus Config</title></head>");
  page_append("<body style=\"font-family:Arial,sans-serif;max-width:980px;margin:20px auto;\">"
              "<h1>Modbus Configuration</h1>");

  render_port_section();
  render_slave_section();
  render_register_section();

  page_append("<p><a href=\"/config.html\">Network config</a></p>");
  page_append("<p><a href=\"/\">Back to main page</a></p>");
  page_append("</body></html>");

  netconn_write(conn, g_app_config_page_buffer, g_page_used, NETCONN_COPY);
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
    char v_baud[20];
    char v_stop[8];
    char v_parity[8];
    char v_port[8];
    unsigned long baud;
    unsigned long stop_bits;
    unsigned long parity;
    unsigned long port_id;

    if ((!get_param(body, "baud", v_baud, sizeof(v_baud))) ||
        (!get_param(body, "stop_bits", v_stop, sizeof(v_stop))) ||
        (!get_param(body, "parity", v_parity, sizeof(v_parity))) ||
        (!get_param(body, "port_id", v_port, sizeof(v_port)))) {
      send_status_page(conn, 0, "Port fields are missing");
      return;
    }

    if ((!parse_ulong(v_baud, &baud)) || (!parse_ulong(v_stop, &stop_bits)) ||
        (!parse_ulong(v_parity, &parity))) {
      send_status_page(conn, 0, "Port fields are invalid numbers");
      return;
    }

    if (!parse_ulong(v_port, &port_id)) {
      send_status_page(conn, 0, "Port id is invalid");
      return;
    }

    if ((port_id >= (unsigned long)MAX_UART_PORTS) || (baud == 0ul) || (baud > 2000000ul) ||
        ((stop_bits != 1ul) && (stop_bits != 2ul)) || (parity > (unsigned long)PARITY_ODD) ||
        (appConfig_updatePort((uartPortId_t)port_id, (uint32_t)baud, (uint8_t)stop_bits,
                              (parityType_t)parity) < 0)) {
      send_status_page(conn, 0, "Port update failed");
      return;
    }
    if (appConfig_save() < 0) {
      send_status_page(conn, 0, "Save failed after port update");
      return;
    }
    send_status_page(conn, 1, "Port updated and saved");
    return;
  }

  if (strcmp(action, "add_slave") == 0) {
    char v_addr[8];
    char v_port[8];

    if ((!get_param(body, "slave_address", v_addr, sizeof(v_addr))) ||
        (!get_param(body, "slave_port", v_port, sizeof(v_port)))) {
      send_status_page(conn, 0, "Slave fields are missing");
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

    if (appConfig_addSlave((uint8_t)n1, (uartPortId_t)n2) < 0) {
      send_status_page(conn, 0, "Cannot add slave (duplicate or full table)");
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
