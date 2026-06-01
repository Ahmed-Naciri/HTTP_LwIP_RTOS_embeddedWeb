<!-- # Algorithme — Appliquer configuration UART (safe)

But : appliquer une nouvelle configuration série depuis l'interface web, l'appliquer réellement sur le hardware, la persister et garantir rollback en cas d'erreur.

---

**Fichier cible / emplacement d'implémentation**
- `Core/Inc/rs485_interface.h` : déclarations `rs485_interface_apply_config` et `rs485_interface_restore_config` (si besoin).
- `Core/Src/rs485_interface.c` : implémentation des apply/restore.
- `Core/Src/app_config.c` : appel de l'algorithme depuis `appConfig_updatePort()` et `appConfig_load()`.
- `Core/Src/app_config_http.c` : handler POST `save_port` doit appeler `appConfig_updatePort()` et retourner le statut.
- `Core/Src/modbus_rtu_master.c` : fournir `modbusMaster_onUartReconfig()` pour réinitialiser le maître après reconfig.

---

## 1) Pseudo‑algorithme (style algorithmique, prêt à traduire)

Algorithme AppliquerConfigUART
{But : appliquer au hardware et persister une nouvelle config UART de façon sûre}

VARIABLES
- portId : entier
- newBaud, newStop, newParity : entiers
- prevRam : portConfig_t
- prevHw : UART_InitTypeDef
- status : entier
- TIMEOUT_MS ← 1000
- MUTEX_APP (mutex global pour `appDb`)

DEBUT
  1. Lire `portId`, `newBaud`, `newStop`, `newParity` depuis la requête HTTP.
  2. Si !ValidatePortParams(portId,newBaud,newStop,newParity) ALORS
       retourner STATUS_VALIDATION_FAILED.
  3. Prendre MUTEX_APP (lock).
  4. prevRam ← copier(appDb.ports[portId]).
  5. prevHw ← snapshot_hw_uart().
  6. temp ← construire nouvelle structure portConfig {used=1, baud=newBaud, stop=newStop, parity=newParity}.
  7. // Appliquer au hardware
     status ← rs485_interface_apply_config(newBaud,newStop,newParity, TIMEOUT_MS).
  8. SI status ≠ HAL_OK ALORS
       a) rs485_interface_restore_config(prevHw) // tenter rollback matériel
       b) appDb.ports[portId] ← prevRam
       c) Libérer MUTEX_APP
       d) retourner STATUS_HW_APPLY_FAILED
  9. // Mise à jour du maître Modbus
     modbusMaster_onUartReconfig()
 10. // Persister
     SI appConfig_save() < 0 ALORS
         tenter 1 retry de appConfig_save()
         SI toujours échoue ALORS
             Loger erreur (PERSIST_FAILED)
             Libérer MUTEX_APP
             retourner STATUS_PERSIST_FAILED
 11. appDb.ports[portId] ← temp  // confirmer RAM (si pas déjà fait)
 12. Libérer MUTEX_APP
 13. retourner STATUS_OK
FIN

---

## 2) Fonctions utilitaires (spécification)

- `bool ValidatePortParams(portId, baud, stop, parity)`
  - Vérifie `portId < MAX_UART_PORTS`, `baud>0 && baud<=2000000`, `stop ∈ {1,2}`, `parity ∈ {PARITY_NONE, PARITY_EVEN, PARITY_ODD}`.
  - Retourne `true/false`.

- `UART_InitTypeDef snapshot_hw_uart(void)`
  - Retourne une copie de `huart3.Init` pour rollback.

- `HAL_StatusTypeDef rs485_interface_apply_config(uint32_t baud, uint8_t stopBits, parityType_t parity, uint32_t timeout_ms)`
  - Attendu :
    1. `HAL_UART_Abort(&huart3)`
    2. `HAL_UART_DeInit(&huart3)`
    3. Modifier `huart3.Init.{BaudRate, StopBits, Parity, WordLength}` (WordLength peut dépendre de Parity)
    4. `HAL_UART_Init(&huart3)`
    5. (Ré)initialiser DMA RX si utilisé (`HAL_UART_Receive_DMA`)
    6. `rs485_interface_enableRxMode()`
  - Retour : `HAL_OK` ou `HAL_ERROR` selon résultat.

- `HAL_StatusTypeDef rs485_interface_restore_config(UART_InitTypeDef prevHw)`
  - Revenir en arrière : abort, deinit, assigner `huart3.Init = prevHw`, init, relancer DMA.

- `void modbusMaster_onUartReconfig(void)`
  - Stoppe temporisation/attente en cours, réinitialise états internes du maître (ou appelle `modbusMaster_init()` de façon contrôlée), réactive lectures.

- `int appConfig_save(void)`
  - Persistance existante : utilise `persistent_store_save_app(&appDb)`.

---

## 3) Diagramme (Mermaid) — visualisation colorée

```mermaid
flowchart TD
  A[Requête Web: save_port] --> B{Valide champs?}
  B -- Non --> Z[Retour: VALIDATION_FAILED]
  B -- Oui --> C[Prendre mutex; snapshot RAM+HW]
  C --> D[Appeler rs485_interface_apply_config]
  D -- Échec --> E[rs485_interface_restore_config]
  E --> F[Restaurer RAM; Libérer mutex]
  F --> Z2[Retour: HW_APPLY_FAILED]
  D -- Succès --> G[modbusMaster_onUartReconfig]
  G --> H[appConfig_save()] 
  H -- Échec --> I[Retry persist une fois]
  I -- Échec --> J[Log PERSIST_FAILED; Retour PERSIST_FAILED]
  H -- Succès --> K[Libérer mutex; Retour OK]

  classDef ok fill:#e6ffed,stroke:#1a7f37;color:#0b3b1a;
  classDef err fill:#ffecec,stroke:#b30000;color:#660000;
  class B,C,D,E,F,G,H,I,J,K ok;
  class Z,Z2 err;
```

---

## 4) Checklist de test minimal (exécutable)

1. Initialiser simulateur slave (9600,N,1). Tester lecture registre → doit réussir.
2. Dans UI, passer à 19200,N,1 → relancer simulateur en 19200 → tester lecture registre → doit réussir.
3. Simuler échec matériel (débrancher bus) et soumettre changement → l'API doit retourner `HW_APPLY_FAILED` et MCU doit rester sur ancienne config.
4. Effectuer changement valide → redémarrer MCU → vérifier que config persiste.

---

## 5) Remarques d'implémentation
- Faire attention à la gestion DMA RX/TX : arrêter avant `DeInit` et relancer après `Init`.
- Modifier `WordLength` si `Parity != PARITY_NONE` (HAL requiert souvent 9 bits si parity enabled).
- `ee_write()` est lent : si l'UI attend, tu peux envisager mode asynchrone (worker queue). Pour commencer, conserver écriture synchrone.
- Toujours loger les erreurs sur une console série ou buffer pour debug.

---

Fichier créé : `docs/uart_apply_algorithm.md`. Traduits cela directement en C en suivant l'algorithme ci‑dessus ; si tu veux, envoie ton code et je corrige la syntaxe et les appels HAL spécifiquement. -->
