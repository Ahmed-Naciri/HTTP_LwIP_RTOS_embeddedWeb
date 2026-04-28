# Explication technique du flux de configuration

## Objectif

Ce document explique comment le projet stocke, charge, valide et applique :

1. la configuration reseau,
2. la configuration Modbus RTU,
3. la configuration des esclaves et des registres,
4. le flux complet entre la page web, la RAM et la persistance Flash.

L'idee principale est la suivante :

- la page web modifie les valeurs en HTTP,
- le code valide les donnees,
- les donnees sont copiees dans les structures runtime,
- la persistance ecrit un bloc commun en EEPROM emulee,
- au reboot, le boot charge ce bloc et reapplique les valeurs.

## Vue d'ensemble de l'architecture

Le projet separe clairement deux domaines fonctionnels :

- reseau IPv4 statique,
- configuration applicative Modbus.

Leur logique est similaire, mais ils n'utilisent pas exactement les memes structures.

- La configuration reseau vit dans `network_config_t`.
- La configuration Modbus vit dans `appDataBase_t`.
- Les deux sont stockees ensemble dans un bloc persistant unique gere par `persistent_store.c`.

Les fichiers les plus importants sont :

- [Core/Src/main.c](../Core/Src/main.c)
- [Core/Src/network_config.c](../Core/Src/network_config.c)
- [Core/Src/app_config.c](../Core/Src/app_config.c)
- [Core/Src/persistent_store.c](../Core/Src/persistent_store.c)
- [Core/Src/httpserver-netconn.c](../Core/Src/httpserver-netconn.c)
- [Core/Src/network_config_http.c](../Core/Src/network_config_http.c)
- [Core/Src/app_config_http.c](../Core/Src/app_config_http.c)

## Flux global

### 1. Boot

Au demarrage, `StartDefaultTask()` charge d'abord les donnees sauvegardees :

- `network_config_load()` pour le reseau,
- `appConfig_load()` pour la configuration Modbus.

Ensuite, `MX_LWIP_Init()` applique les valeurs reseau chargees, puis `http_server_netconn_init()` demarre le serveur web.

En pratique, l'ordre est important :

1. charger la configuration,
2. appliquer la configuration,
3. demarrer le reseau,
4. demarrer l'interface web.

Si on inverse cet ordre, on peut demarrer avec des valeurs par defaut ou perdre la coherence entre ce qui est stocke et ce qui est actif.

### 2. Affichage dans la page web

Le serveur HTTP expose deux pages :

- `/config.html` pour la configuration reseau,
- `/modbus_config.html` pour la configuration Modbus.

Chaque page appelle son propre generateur HTML :

- `network_config_http_send_form()`
- `app_config_http_send_form()`

Ces fonctions lisent les structures runtime et remplissent le formulaire HTML avec les valeurs actuelles.

### 3. Sauvegarde depuis la page web

Quand l'utilisateur clique sur Save :

- la page reseau envoie `POST /save_config`,
- la page Modbus envoie `POST /save_modbus_config`.

Le routeur HTTP dans `httpserver-netconn.c` detecte l'URL, appelle le bon handler, puis le handler :

1. extrait les champs du body HTTP,
2. decode les donnees URL-encoded,
3. verifie les valeurs,
4. met a jour la structure runtime,
5. ecrit le bloc persistant.

### 3bis. Application materielle des changements serie

Pour les parametres UART, la sauvegarde seule ne suffit pas. Le code applique aussi la configuration sur le hardware via une reinitialisation de `USART3`.

Le flux est le suivant :

1. la page web envoie le nouveau `baud`, `stop_bits` et `parity`,
2. `appConfig_updatePort()` met a jour `appDb`,
3. `rs485_interface_apply_config()` reconfigure `huart3`,
4. `appConfig_save()` persiste la nouvelle valeur dans la Flash emulatee.

Effet attendu : le changement devient reel sur le port serie sans devoir modifier le firmware manuellement.

### 4. Relecture au reboot

Au redemarrage, `persistent_store.c` lit la Flash emulee.

Si la zone est vide ou invalide :

- des valeurs par defaut sont reconstruites,
- puis re-ecrites dans la persistance.

Si la zone est valide :

- le bloc est copie dans la RAM,
- les structures runtime sont remplies,
- le systeme repart avec la derniere configuration sauvegardee.

## Structure de persistance

Le coeur du stockage est dans `persistent_store_data_t` :

- un marqueur global `magic`,
- une configuration reseau,
- un marqueur applicatif `app_magic`,
- une base applicative `app`.

Ce choix permet de stocker deux zones logiques dans un seul bloc physique.

### Pourquoi deux magic numbers

Il y a deux niveaux de validation :

- `PERSISTENT_STORE_MAGIC` protege l'ensemble du bloc,
- `APP_CONFIG_MAGIC` protege la sous-partie applicative.

Cette separation permet de detecter :

- une Flash vierge,
- une corruption partielle,
- une ancienne version de donnees,
- une structure applicative invalide.

## Detailles du flux reseau

### Data model

La configuration reseau est definie dans `network_config_t` :

- `magic` : marqueur de validite,
- `ip[4]` : adresse IPv4,
- `netmask[4]` : masque,
- `gateway[4]` : passerelle.

### Initialisation par defaut

`network_config_set_defaults()` remplit des valeurs de secours :

- IP : 192.168.1.222,
- masque : 255.255.255.0,
- gateway : 192.168.1.1.

Ces valeurs servent de base quand aucune config valide n'existe.

### Chargement

`network_config_load()` fait trois choses :

1. initialise la RAM avec les defaults,
2. demande a `persistent_store_load_network()` de lire les donnees sauvegardees,
3. si le bloc lu est invalide, il restaure les defaults et les sauvegarde.

### Sauvegarde

`network_config_save()` :

1. copie l'objet passe en parametre,
2. force `magic = NETWORK_CONFIG_MAGIC`,
3. sauvegarde via `persistent_store_save_network()`,
4. met a jour `g_network_config` en RAM.

Ce fonctionnement garantit que la valeur persistante et la valeur runtime restent synchronisees.

### Application vers lwIP

`MX_LWIP_Init()` lit `g_network_config` et construit les objets lwIP :

- `ipaddr`,
- `netmask`,
- `gw`.

Ensuite, `netif_add()` et `netif_set_up()` rendent le lien utilisable par la pile reseau.

## Detailles du flux Modbus

### Data model

La configuration Modbus est stockee dans `appDataBase_t`.

Elle contient :

- un tableau `ports[]` pour les ports UART,
- un tableau `slaveConfig[]` pour les esclaves,
- pour chaque esclave, un tableau `registerConfig[]` pour les registres.

### Port serie

Chaque `portConfig_t` contient :

- `used` : indique si le port est actif,
- `portId` : identifiant du port,
- `baudRate` : vitesse serie,
- `stopBits` : 1 ou 2,
- `parity` : none, even ou odd.

### Esclaves

Chaque `slaveConfig_t` contient :

- `used` : esclave actif ou non,
- `slaveAddress` : adresse Modbus,
- `portId` : port serie associe,
- `registerCount` : nombre de registres actifs,
- `registerConfig[]` : liste des registres.

### Registres

Chaque `registerConfig_t` contient :

- `used` : registre actif ou non,
- `regAddress` : adresse du registre,
- `registerType` : U16, I16 ou FLOAT,
- `lastValue` : derniere valeur lue ou ecrite,
- `valid` : marqueur d'etat de la valeur.

## Validation Modbus

La fonction `appConfig_isValid()` verifie que la structure est coherente avant sauve garde.

Elle controle notamment :

- les champs `used` doivent valoir 0 ou 1,
- `portId` doit appartenir a une valeur valide,
- `stopBits` doit etre 1 ou 2,
- `parity` doit etre dans l'intervalle autorise,
- une adresse esclave doit etre entre 1 et 247,
- un registre ne peut pas exister sans esclave actif,
- le nombre de registres actifs doit correspondre a `registerCount`.

Cette validation est importante car la persistance ecrit un bloc complet, donc une erreur de structure peut corrompre toute la configuration applicative.

## Initialisation Modbus

`appConfig_setDefaults()` remet toute la base applicative dans un etat propre :

- tous les ports sont remis a zero ou a des valeurs de base,
- tous les esclaves sont marques inactifs,
- tous les registres sont remis a l'etat vierge.

`appConfig_defaultInit()` appelle simplement cette fonction sur `appDb` global.

### Chargement au boot

`appConfig_load()` :

1. remet `appDb` sur des defaults de secours,
2. lit la partie applicative de la persistance,
3. valide le bloc charge,
4. recharge des defaults si le bloc est invalide.

### Sauvegarde

`appConfig_save()` :

1. valide `appDb`,
2. ecrit vers `persistent_store_save_app()`,
3. retourne 0 si tout s'est bien passe.

## Flux HTTP reseau 

### Lecture de la requete

Le serveur HTTP dans `httpserver-netconn.c` lit la requete brute dans un buffer et attend, si besoin, des segments supplementaires pour reconstruire tout le body POST.

Cette etape est importante car un navigateur peut envoyer une requete en plusieurs morceaux TCP.

### Route reseau

Si la requete est `POST /save_config`, le serveur appelle :

- `network_config_http_handle_save()`.

Cette fonction :

1. recupere le body,
2. extrait les champs `ip`, `netmask`, `gateway`,
3. decode les caracteres URL-encodes,
4. parse les 4 octets de chaque IPv4,
5. appelle `network_config_save()`.

### Pourquoi cette route est robuste

Parce qu'elle ne fait pas confiance au texte brut du navigateur.

Elle accepte :

- le format x-www-form-urlencoded,
- les espaces parasites,
- les retours chariot,
- les caracteres encodes avec `%xx`.

## Flux HTTP Modbus

### Page de configuration

`app_config_http_send_form()` construit la page HTML avec :

- baud rate,
- stop bits,
- parity,
- liste des slaves,
- liste des registres.

Les listes sont reconstruites a partir de `appDb`.

La page utilise un buffer HTML statique de grande taille pour eviter une surcharge de pile dans le thread HTTP.

### Soumission du formulaire

Quand l'utilisateur soumet la page, `app_config_http_handle_save()` recupere :

- `action`,
- `baud`,
- `stop_bits`,
- `parity`,
- `port_id`,
- `slave_address`,
- `slave_port`,
- `slave_index`,
- `reg_slave_index`,
- `reg_address`,
- `reg_type`.

Ensuite le handler :

1. verifie les valeurs numeriques,
2. appelle la fonction cible de `app_config.c`,
3. sauvegarde immediatement avec `appConfig_save()`,
4. retourne une page de statut HTML.

### Format attendu

Le formulaire Modbus courant ne depend pas d'un champ multi-ligne pour les slaves ou les registres. Il utilise des actions distinctes :

- `save_port`
- `add_slave`
- `delete_slave`
- `add_register`
- `delete_register`

Chaque action reconstruit directement l'etat runtime en RAM puis ecrit la Flash.

## Version finale actuelle du code

Cette section resume les changements techniques effectivement presents dans la version finale.

### 1. HTTP reseau plus robuste

Le fichier `httpserver-netconn.c` assemble maintenant la requete complete avant le routage. Cela evite les erreurs quand le navigateur envoie les headers ou le body en plusieurs segments TCP.

Technique utilisee :

1. lecture du premier bloc avec `netbuf_copy_partial()`,
2. recherche de la fin des headers `\r\n\r\n`,
3. continuation de la reception jusqu'a obtenir un body complet ou jusqu'a la fin du buffer,
4. appel du routeur avec la requete complete.

Effet :

- moins de faux retours lents,
- moins de parsings incomplets,
- comportement plus stable avec les navigateurs.

### 2. Page reseau plus simple et plus legere

Le fichier `network_config_http.c` utilise :

- un buffer HTML statique,
- un template `CONFIG_PAGE_TEMPLATE`,
- un parseur IPv4 strict,
- un decode URL in place,
- la lecture des valeurs runtime depuis `gnetif` quand le lien est actif.

Le champ affiche dans le formulaire n'est donc pas seulement la derniere valeur sauvegardee. Si le lien est monte, la page montre aussi les valeurs vraiment utilisees par la pile reseau.

### 3. Page Modbus rendue avec helpers reutilisables

Le fichier `app_config_http.c` construit la page en petits blocs :

- section port,
- section slaves,
- section registers.

Les helpers principaux sont :

- `page_reset()` : remet le buffer HTML a zero,
- `page_append()` : ajoute du texte,
- `page_append_uint()` : ajoute un entier sans allocation,
- `get_param()` : lit un champ HTML,
- `parse_ulong()` : parse un nombre decimal,
- `url_decode_inplace()` : decode les valeurs form-url-encoded.

Le code reste en sauvegarde immediate sur chaque action pour garder un comportement simple et previsible.

Les changements de port serie sont en plus appliques sur le hardware immediatement apres validation.

### 4. Persistance commune centralisee

Le fichier `persistent_store.c` gere un seul bloc Flash qui contient :

- la configuration reseau,
- la base Modbus,
- deux marqueurs de validite,
- des defaults de secours si la Flash est vide ou invalide.

Les fonctions importantes sont :

- `persistent_store_ensure_loaded()` : charge et valide le bloc,
- `persistent_store_load_network()` : copie le reseau vers la RAM,
- `persistent_store_save_network()` : ecrit le reseau en Flash,
- `persistent_store_load_app()` : copie le Modbus vers la RAM,
- `persistent_store_save_app()` : ecrit le Modbus en Flash.

### 5. Application Modbus complete dans `app_config.c`

Les fonctions d'edition de la base Modbus font maintenant le travail complet :

- ajout d'un slave,
- suppression d'un slave avec suppression en cascade des registres,
- ajout d'un registre,
- suppression d'un registre,
- mise a jour d'un port serie.

La logique de suppression est importante : quand un slave est retire, ses registres associes sont nettoyes aussi, pour garder `registerCount` coherent.

## Problemes rencontres et solutions

### 1. Requetes HTTP fragmentes

Probleme : le navigateur pouvait envoyer les headers et le body en plusieurs morceaux TCP, ce qui rendait le parsing incomplet.

Solution : `httpserver-netconn.c` attend maintenant la fin des headers et continue la reception tant que la requete n'est pas complete.

### 2. Lenteur percue au chargement des pages

Probleme : le rendu HTTP etait sensible aux requetes incomplètes et aux buffers de pile trop grands.

Solution :

- buffers HTML statiques pour les pages,
- rendu en morceaux simples,
- requete complete avant routage.

### 3. Sauvegarde Flash qui bloque le flux web

Probleme : chaque ecriture Flash peut prendre du temps car l'EEPROM emulee fait erase/program/verify.

Solution : garder une logique simple et directe dans la version finale, puis documenter clairement le flux pour ne pas masquer le cout reel de la persistance.

### 4. Donnees Modbus incoherentes apres suppression

Probleme : supprimer un slave sans nettoyer ses registres aurait pu laisser `registerCount` ou les entrees internes incoherentes.

Solution : la fonction de suppression du slave efface aussi tous les registres associes.

### 5. Fichiers difficiles a relire

Probleme : le code etait devenu difficile a comprendre sans suivre chaque fonction.

Solution : ajouter des commentaires techniques en anglais dans le code pour expliquer le role des fonctions et des blocs moins evidents.

## Ce qu'il faut retenir

La version finale du projet repose sur quatre idees simples :

1. La page web construit une requete claire.
2. Le serveur HTTP la recoit en entier avant de parser.
3. Les handlers Modbus et reseau mettent a jour la RAM puis sauvegardent dans la Flash.
4. Le boot recharge la Flash avant d'initialiser le reseau.

Cette organisation donne un comportement plus previsible et plus facile a maintenir.

## Flux de persistance commun

### Chargement unique

`persistent_store_ensure_loaded()` est le point central.

Il fait :

1. `ee_init()` pour brancher le backend EEPROM emulee,
2. `ee_read()` pour lire le bloc brut,
3. verification du magic global,
4. verification de la partie reseau,
5. verification de la partie applicative,
6. reconstruction des defaults si necessaire,
7. `ee_write()` si une correction a ete faite.

### Sauvegarde reseau

`persistent_store_save_network()` copie la config reseau dans le bloc global, met a jour le magic, puis appelle `ee_write()`.

### Sauvegarde applicative

`persistent_store_save_app()` copie `appDb` dans le bloc global, met a jour `app_magic`, puis appelle `ee_write()`.

### Point important

Ce design stocke reseau et Modbus dans la meme zone Flash physique, mais sous deux sous-structures differentes.

Cela simplifie la coherence du boot, mais impose de toujours ecrire le bloc global complet lorsque l'un des deux sous-blocs change.

## Pourquoi `persistent_store` est utile

`persistent_store` sert de couche commune entre la Flash emulee et les vraies structures runtime.

Son interet principal est de centraliser dans un seul endroit :

- la lecture Flash,
- la verification des marqueurs de validite,
- la reconstruction des defaults,
- l'ecriture Flash,
- la coherence entre la partie reseau et la partie Modbus.

Sans cette couche, chaque module devrait refaire lui-meme une partie de ce travail. On aurait alors :

- plus de code duplique,
- plus de risques d'oubli de validation,
- plus de risques d'incoherence entre `network_config.c` et `app_config.c`,
- plus de difficultes pour faire evoluer le format de stockage.

### Est-ce qu'on peut travailler sans `persistent_store` ?

Oui, techniquement c'est possible.

On pourrait faire charger et sauvegarder directement chaque module via `ee_read()` et `ee_write()`. Mais dans ce cas :

- `network_config.c` devrait gerer toute seule la lecture Flash,
- `app_config.c` devrait gerer toute seule la lecture Flash,
- chaque module devrait verifier ses propres magic numbers,
- chaque module devrait gerer la Flash vide ou corrompue,
- la logique de correction serait repartie un peu partout.

Donc le projet peut fonctionner sans cette couche, mais il devient plus fragile, plus long a maintenir, et plus facile a casser quand on ajoute une nouvelle configuration.

En pratique, `persistent_store` est surtout utile quand on veut :

- garder un boot simple,
- conserver un seul format de persistance,
- synchroniser plusieurs configurations dans une seule zone Flash,
- limiter les erreurs de maintenance.

## Fonctions de `persistent_store`

### Fonctions publiques

#### `persistent_store_load_network(network_config_t *cfg)`

Cette fonction copie la configuration reseau persistée dans `cfg`.

Elle verifie d'abord que :

- `cfg` n'est pas `NULL`,
- la persistance est chargee et validee,
- le bloc interne est pret a etre lu.

Elle est utilisee par `network_config_load()`.

#### `persistent_store_save_network(const network_config_t *cfg)`

Cette fonction met a jour la partie reseau du bloc persistant, force le `magic` reseau, puis ecrit le bloc complet en Flash.

Elle est utilisee par `network_config_save()`.

#### `persistent_store_load_app(appDataBase_t *db)`

Cette fonction copie la configuration Modbus persistée dans `db`.

Elle est utilisee par `appConfig_load()`.

#### `persistent_store_save_app(const appDataBase_t *db)`

Cette fonction met a jour la partie Modbus du bloc persistant, force `app_magic`, puis ecrit le bloc complet en Flash.

Elle est utilisee par `appConfig_save()`.

### Fonctions internes au fichier

#### `persistent_store_set_defaults(persistent_store_data_t *store)`

Cette fonction interne remplit le bloc persistant avec des valeurs de secours propres.

Elle :

- force le magic global,
- remet les valeurs reseau par defaut,
- remet la base Modbus par defaut,
- force le magic applicatif.

Elle n'est pas exposee dans le header car elle sert seulement au mecanisme interne de secours.

#### `persistent_store_ensure_loaded()`

Cette fonction interne est le coeur du mecanisme.

Elle fait tout le travail suivant :

1. initialise le backend EEPROM emulee avec `ee_init()`,
2. lit le bloc Flash avec `ee_read()`,
3. verifie si le magic global est valide,
4. restaure les defaults si la Flash est vierge,
5. verifie la partie reseau,
6. verifie la partie Modbus,
7. re-ecrit la Flash si une correction a ete necessaire.

Elle n'est pas exposee dans le header parce que les autres fichiers n'ont pas besoin de l'appeler directement.

### Resume simple

On peut voir `persistent_store` comme un "gestionnaire central" de la persistance.

Les autres modules ne parlent pas directement a `ee.c`. Ils parlent a `persistent_store`, qui s'occupe du reste.

## Points de vigilance

1. Ne pas modifier la taille ou l'ordre de `persistent_store_data_t` sans migrer les donnees.
2. Ne pas oublier d'appeler `network_config_load()` et `appConfig_load()` avant d'initialiser les services qui en dependent.
3. Ne pas accepter des valeurs hors plage dans les handlers HTTP.
4. Ne pas casser le lien entre `registerCount` et le nombre reel de registres actives.
5. Ne pas changer la cartographie Flash de l'EEPROM emulee sans verifier `ee_config.h`.

## Resume operationnel

Le cycle complet est :

1. Boot.
2. Lecture Flash.
3. Reconstitution des structures reseau et Modbus.
4. Initialisation lwIP.
5. Demarrage du serveur HTTP.
6. Affichage des formulaires web.
7. Soumission des modifications.
8. Validation et sauvegarde.
9. Reboot.
10. Rechargement automatique des valeurs sauvegardees.

## Checklist de test materiel

Pour valider le comportement complet, il faut tester dans cet ordre :

1. configurer un slave simule sur le PC avec le bon `baud`, `parity` et `stop bits`,
2. connecter le MCU au simulateur via RS485,
3. creer ou modifier un esclave depuis l'interface web,
4. verifier que la lecture de registres fonctionne vraiment,
5. changer le `baud` depuis le web et verifier que l'UART du MCU suit bien ce nouveau parametre,
6. rebooter et confirmer que la valeur persiste dans la Flash.

Si une lecture echoue apres changement de config, le premier point a verifier est toujours la coherence entre le port serie du MCU et celui du simulateur.

## Conclusion

Le projet utilise maintenant un schema unique et coherent :

- les donnees sont configurees depuis le web,
- les donnees sont validees dans la couche applicative,
- les donnees sont stockees dans une EEPROM emulee,
- les donnees sont rechargees au boot avant l'initialisation reseau,
- le tout reste separe entre domaine reseau et domaine Modbus, mais partage un meme backend de persistance.

Ce fonctionnement est propre, maintenable et suffisamment robuste pour des ajouts futurs comme :

- plusieurs ports UART,
- plusieurs pages de configuration,
- un format plus structure pour les slaves et registres,
- des pages de lecture seule pour le diagnostic.

## Derniers changements et probleme resolus

Dans la version finale du projet, nous avons surtout garde les modifications de documentation et de clarification du code. Le comportement fonctionnel du Modbus est reste simple :

- chaque action importante valide d'abord les valeurs,
- puis met a jour la structure runtime,
- puis sauvegarde immediatement la configuration dans la Flash emulee.

La variante avec sauvegarde differee a ete etudiee pour la rapidite, mais elle n'a pas ete retenue dans le code final afin de ne pas perturber le comportement deja en place.

### Problemes rencontres pendant le debug

1. Requetes HTTP parfois decoupees en plusieurs segments TCP.
	- Solution : assembler la requete complete avant de parser les champs HTTP.

2. Affichage lent ou comportement instable quand le serveur web recevait une requete incomplete.
	- Solution : attendre le debut et la fin du body HTTP avant de lancer le routage.

3. Sauvegarde Flash percue comme lente.
	- Solution : garder un chemin clair et stable, documenter le flux, et ne pas ajouter de logique complexe sans validation.

4. Code difficile a relire pour comprendre le role de chaque fonction.
	- Solution : ajouter des commentaires en anglais dans le code pour expliquer les fonctions principales et les blocs moins evidents.

### Ce que les documents doivent maintenant expliquer

Les documents doivent montrer clairement :

- le role de chaque couche,
- le chemin complet entre la page web, la RAM et la persistance,
- les problemes observes,
- et la solution retenue pour chaque probleme.

En pratique, cela permet de reprendre le projet plus tard sans devoir refaire toute l'analyse depuis le debut.