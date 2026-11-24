(function () {
  'use strict';

  var TRANSLATIONS = {
    fr: {
      pageTitle: 'Agenda — Calendrier natif Windows en C Win32',
      pageDesc:
        'Application de calendrier native Windows écrite en C Win32 Unicode, sans dépendance externe, avec stockage JSON versionné, thème clair/sombre et notifications tray.',
      navFeatures: 'Fonctionnalités',
      navPreview: 'Aperçu',
      navArchitecture: 'Architecture',
      navBuild: 'Compilation',
      heroMeta: 'C Win32 Unicode · Zéro dépendance · MinGW',
      heroTitle: 'Un agenda natif Windows, léger et entièrement local.',
      heroDesc:
        'Écrit en C Win32 Unicode pur sans framework ni moteur web embarqué. Retrouvez vos vues Jour, Semaine, Mois et Année, vos récurrences avancées, vos anniversaires avec calcul d’âge, le thème système Windows et un historique JSON horodaté.',
      ctaPrimary: 'Compiler avec MinGW',
      ctaSecondary: 'Explorer les fonctions',
      specLangLabel: 'Code source',
      specLangValue: 'C Win32 Unicode',
      specStorageLabel: 'Données & réglages',
      specStorageValue: 'JSON + agenda.ini',
      specDepLabel: 'Dépendances externes',
      specDepValue: 'Aucune',
      previewDarkLabel: 'Thème sombre',
      previewLightLabel: 'Thème clair',
      tabDay: 'Jour',
      tabWeek: 'Semaine',
      tabMonth: 'Mois',
      tabYear: 'Année',
      toastApp: 'Agenda',
      toastTitle: '[09:00] Marie Dupont (36 ans)',
      toastBody: 'Anniversaire : 36 ans — Appeler à midi',
      featuresTitle: 'Conçu pour rester rapide, lisible et sous votre contrôle.',
      featuresDesc:
        'Chaque composant d’interface et de persistance utilise directement les API Windows standard, sans inscription dans le Registre.',
      f1Kicker: '01. Vues complètes & édition directe',
      f1Title: 'Jour, Semaine, Mois et Année',
      f1Body:
        'Passez instantanément d’une grille horaire quotidienne ou hebdomadaire à une vue mensuelle ou annuelle 4×3. En vue Année, un clic sur une pastille colorée ouvre directement l’événement ou le menu du jour.',
      f1L1: 'Filtrage instantané par mot-clé dans la barre d’outils',
      f1L2: 'Ligne rouge d’heure courante et gestion des chevauchements',
      f1L3: 'Sélecteurs thémés Date, Année (molette & clavier) et Heure',
      f2Kicker: '02. Récurrences & anniversaires',
      f2Title: 'Calcul automatique de l’âge',
      f2Body:
        'Planifiez des événements uniques ou répétés par jour, semaine, date fixe du mois, nième jour (ex. premier lundi du mois), année ou en mode Anniversaire.',
      f2L1: 'Affichage de l’âge calculé dans le calendrier et les notifications',
      f2L2: 'Gestion des années bissextiles (29 février)',
      f2L3: 'Sélecteur d’année dédié pour saisir rapidement une année de naissance',
      f3Kicker: '03. Stockage transparent',
      f3Title: 'JSON lisible & snapshots illimités',
      f3Body:
        'Choisissez le dossier de vos données. Chaque création, modification ou suppression génère automatiquement une sauvegarde JSON horodatée dans un sous-dossier versions\\.',
      f3L1: 'Paramètres locaux dans agenda.ini (zéro écriture dans le Registre)',
      f3L2: 'Conservation de la police personnalisée et de la vue active',
      f3L3: 'Restauration simple par copie de fichier JSON',
      f4Kicker: '04. Intégration Windows native',
      f4Title: 'Thème système, tray & notifications',
      f4Body:
        'Détection en direct du mode clair ou sombre Windows, barre de titre et dialogues adaptés, icône dans la zone de notification et démarrage optionnel par raccourci .lnk.',
      f4L1: 'Notifications Windows natives avec description et son .wav personnalisé',
      f4L2: 'Démarrage réduit et minimisation dans le tray',
      f4L3: 'Raccourci .lnk créé uniquement dans le dossier Démarrage Windows',
      archTitle: 'Des fichiers clairs, lisibles et faciles à sauvegarder.',
      archDesc:
        'Aucune base binaire propriétaire : inspectez vos événements et vos réglages avec n’importe quel éditeur de texte.',
      spec1Title: 'Configuration locale (agenda.ini)',
      spec1Desc:
        'Dossier cible, mode d’affichage, police par défaut, son de rappel et options tray enregistrés à côté de l’exécutable.',
      spec2Title: 'Historique versionné (versions\\)',
      spec2Desc:
        'Chaque modification incrémente un compteur de version et conserve un instantané horodaté prêt à être restauré.',
      spec3Title: 'Identité Windows propre',
      spec3Desc:
        'Ressource de version intégrée, manifeste DPI Per-Monitor v2 et icône multi-résolutions embarquée dans l’exécutable.',
      buildTitle: 'Compiler depuis les sources sous Windows.',
      buildDesc:
        'Le projet utilise exclusivement les bibliothèques système Windows livrées avec MinGW-w64.',
      buildStep1Title: '01. Compiler avec MinGW',
      buildStep1Desc:
        'Exécutez mingw32-make.exe à la racine du dépôt. Les fichiers objets sont générés dans obj\\ et l’exécutable final AgendaWin32.exe est prêt à l’emploi.',
      buildStep2Title: '02. Publier ce site sur GitHub Pages',
      buildStep2Desc:
        'Ce site statique réside dans le dossier pages\\. Configurez GitHub Pages sur la branche principale et le dossier /pages (ou déployez le dossier via GitHub Actions).',
      copyLabel: 'Copier',
      copiedLabel: 'Copié',
      footerText: 'Agenda — Logiciel libre C Win32 Unicode',
      sampleBirthday: 'Marie Dupont (36 ans)',
      sampleMeeting: 'Revue produit',
      sampleDoctor: 'Dentiste',
      sampleTax: 'Premier lundi du mois',
      monthHeader: 'Novembre 2026',
      yearHeader: 'Année 2026',
      weekHeader: 'Semaine du 23 nov. 2026',
      dayHeader: 'Mardi 24 novembre 2026',
      dows: ['Lun', 'Mar', 'Mer', 'Jeu', 'Ven', 'Sam', 'Dim'],
      monthsShort: ['Jan', 'Fév', 'Mar', 'Avr', 'Mai', 'Juin', 'Juil', 'Août', 'Sep', 'Oct', 'Nov', 'Déc']
    },
    en: {
      pageTitle: 'Agenda — Native Windows C Win32 Calendar',
      pageDesc:
        'Zero-dependency native Windows desktop calendar written in pure C Win32 Unicode with versioned JSON snapshots, Light/Dark system theme, and tray notifications.',
      navFeatures: 'Features',
      navPreview: 'Preview',
      navArchitecture: 'Architecture',
      navBuild: 'Build',
      heroMeta: 'C Win32 Unicode · Zero Dependencies · MinGW',
      heroTitle: 'A fast, local-first native Windows calendar.',
      heroDesc:
        'Built in pure C Win32 Unicode with no frameworks or embedded browser runtime. Includes Day, Week, Month, and Year views, flexible recurrences, birthdays with live age calculation, Windows Light/Dark theming, and timestamped JSON history.',
      ctaPrimary: 'Build with MinGW',
      ctaSecondary: 'Explore features',
      specLangLabel: 'Source language',
      specLangValue: 'C Win32 Unicode',
      specStorageLabel: 'Data & settings',
      specStorageValue: 'JSON + agenda.ini',
      specDepLabel: 'External dependencies',
      specDepValue: 'None',
      previewDarkLabel: 'Dark theme',
      previewLightLabel: 'Light theme',
      tabDay: 'Day',
      tabWeek: 'Week',
      tabMonth: 'Month',
      tabYear: 'Year',
      toastApp: 'Agenda',
      toastTitle: '[09:00] Marie Dupont (36 yrs)',
      toastBody: 'Birthday: 36 yrs — Call at noon',
      featuresTitle: 'Engineered for speed, clarity, and data ownership.',
      featuresDesc:
        'Every UI control and storage layer talks directly to standard Windows APIs with zero Registry writes.',
      f1Kicker: '01. Four views & direct editing',
      f1Title: 'Day, Week, Month, and Year',
      f1Body:
        'Switch between hourly Day and Week grids, Month overview, and a 4×3 Year grid. In Year view, clicking any highlighted day opens the event editor or lets you pick from multiple events on that date.',
      f1L1: 'Live search filtering across titles and descriptions',
      f1L2: 'Current-time indicator line and concurrent event lanes',
      f1L3: 'Custom themed Date, Year (wheel & keyboard), and Time pickers',
      f2Kicker: '02. Recurrence & birthdays',
      f2Title: 'Automatic age calculation',
      f2Body:
        'Schedule single events or repeat daily, weekly, monthly by date, Nth weekday (such as first Monday), yearly, or using the dedicated Birthday mode.',
      f2L1: 'Displays calculated age inside calendar views and tray alerts',
      f2L2: 'Handles leap-year birthdays (February 29) gracefully',
      f2L3: 'Dedicated Year Picker makes selecting birth years effortless',
      f3Kicker: '03. Transparent local storage',
      f3Title: 'Readable JSON & unlimited snapshots',
      f3Body:
        'Pick any folder for your calendar data. Every creation, edit, or deletion automatically archives a timestamped JSON snapshot in a versions\\ subfolder.',
      f3L1: 'Portable configuration stored in agenda.ini (zero Registry entries)',
      f3L2: 'Persists custom application font family, size, and active view',
      f3L3: 'Instant recovery by restoring any JSON snapshot file',
      f4Kicker: '04. Native Windows integration',
      f4Title: 'System theme, tray & reminders',
      f4Body:
        'Adapts live to Windows Light/Dark mode, dark title bars and dialogs, minimizes to the notification area, and optionally creates a Startup .lnk shortcut.',
      f4L1: 'Native Windows notifications showing event description and custom .wav audio',
      f4L2: 'Start minimized in tray and minimize-to-tray behavior',
      f4L3: 'Clean .lnk shortcut in Windows Startup folder without touching the Registry',
      archTitle: 'Plain-text files you can read, diff, and back up.',
      archDesc:
        'No opaque binary databases: inspect events, version history, and preferences in any text editor.',
      spec1Title: 'Portable settings (agenda.ini)',
      spec1Desc:
        'Stores your chosen data folder, view mode, default font, notification sound, and tray flags right beside the executable.',
      spec2Title: 'Versioned history (versions\\)',
      spec2Desc:
        'Each change increments a version counter and writes an immutable timestamped JSON snapshot.',
      spec3Title: 'Clean Windows identity',
      spec3Desc:
        'Embeds version metadata, Per-Monitor DPI v2 manifest, and a multi-resolution icon directly into the binary.',
      buildTitle: 'Compile from source on Windows.',
      buildDesc:
        'Links solely against standard Windows system libraries provided by MinGW-w64.',
      buildStep1Title: '01. Build with MinGW',
      buildStep1Desc:
        'Run mingw32-make.exe from the repository root. Object files are placed in obj\\ and the standalone AgendaWin32.exe binary is ready immediately.',
      buildStep2Title: '02. Publish on GitHub Pages',
      buildStep2Desc:
        'This static site lives inside the pages\\ folder. Point GitHub Pages to the main branch /pages folder or deploy pages/ via GitHub Actions.',
      copyLabel: 'Copy',
      copiedLabel: 'Copied',
      footerText: 'Agenda — Pure C Win32 Unicode Calendar',
      sampleBirthday: 'Marie Dupont (36 yrs)',
      sampleMeeting: 'Product review',
      sampleDoctor: 'Dentist',
      sampleTax: 'First Monday check-in',
      monthHeader: 'November 2026',
      yearHeader: 'Year 2026',
      weekHeader: 'Week of Nov 23, 2026',
      dayHeader: 'Tuesday, November 24, 2026',
      dows: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'],
      monthsShort: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']
    },
    es: {
      pageTitle: 'Agenda — Calendario nativo de Windows en C Win32',
      pageDesc:
        'Calendario de escritorio nativo para Windows en C Win32 Unicode sin dependencias, con almacenamiento JSON versionado, tema claro/oscuro y notificaciones en la bandeja.',
      navFeatures: 'Funciones',
      navPreview: 'Vista previa',
      navArchitecture: 'Arquitectura',
      navBuild: 'Compilación',
      heroMeta: 'C Win32 Unicode · Sin dependencias · MinGW',
      heroTitle: 'Un calendario nativo para Windows, rápido y local.',
      heroDesc:
        'Escrito en C Win32 Unicode puro sin frameworks ni navegadores embebidos. Incluye vistas de Día, Semana, Mes y Año, recurrencias flexibles, cumpleaños con cálculo de edad, tema del sistema Windows e historial JSON con marca de tiempo.',
      ctaPrimary: 'Compilar con MinGW',
      ctaSecondary: 'Ver funciones',
      specLangLabel: 'Código fuente',
      specLangValue: 'C Win32 Unicode',
      specStorageLabel: 'Datos y ajustes',
      specStorageValue: 'JSON + agenda.ini',
      specDepLabel: 'Dependencias externas',
      specDepValue: 'Ninguna',
      previewDarkLabel: 'Tema oscuro',
      previewLightLabel: 'Tema claro',
      tabDay: 'Día',
      tabWeek: 'Semana',
      tabMonth: 'Mes',
      tabYear: 'Año',
      toastApp: 'Agenda',
      toastTitle: '[09:00] Marie Dupont (36 años)',
      toastBody: 'Cumpleaños: 36 años — Llamar al mediodía',
      featuresTitle: 'Diseñado para ser ligero, claro y totalmente local.',
      featuresDesc:
        'Cada control y capa de almacenamiento utiliza directamente las API estándar de Windows sin escribir en el Registro.',
      f1Kicker: '01. Cuatro vistas y edición directa',
      f1Title: 'Día, Semana, Mes y Año',
      f1Body:
        'Cambia entre cuadrículas horarias de Día y Semana, vista mensual o un resumen anual 4×3. En vista Año, un clic en un día marcado abre directamente el editor o la lista del día.',
      f1L1: 'Búsqueda instantánea por título y descripción',
      f1L2: 'Línea de hora actual y carriles para eventos simultáneos',
      f1L3: 'Selectores propios de Fecha, Año (rueda y teclado) y Hora',
      f2Kicker: '02. Recurrencias y cumpleaños',
      f2Title: 'Cálculo automático de edad',
      f2Body:
        'Programa eventos únicos o repetidos por día, semana, fecha fija del mes, enésimo día (ej. primer lunes), año o modo Cumpleaños.',
      f2L1: 'Muestra la edad calculada en el calendario y en las notificaciones',
      f2L2: 'Control automático de años bisiestos (29 de febrero)',
      f2L3: 'Selector de año dedicado para elegir fácilmente el año de nacimiento',
      f3Kicker: '03. Almacenamiento transparente',
      f3Title: 'JSON legible y copias ilimitadas',
      f3Body:
        'Elige cualquier carpeta para tus datos. Cada creación, edición o eliminación guarda una copia JSON con fecha y hora en versions\\.',
      f3L1: 'Configuración portátil en agenda.ini (cero entradas en el Registro)',
      f3L2: 'Guarda fuente personalizada, tamaño y vista activa',
      f3L3: 'Restauración sencilla copiando cualquier snapshot JSON',
      f4Kicker: '04. Integración nativa Windows',
      f4Title: 'Tema del sistema, bandeja y avisos',
      f4Body:
        'Detecta en vivo el tema claro u oscuro de Windows, minimiza a la bandeja del sistema y crea opcionalmente un acceso directo .lnk al inicio.',
      f4L1: 'Notificaciones nativas con descripción y sonido .wav personalizado',
      f4L2: 'Inicio minimizado y minimización a la bandeja',
      f4L3: 'Acceso directo .lnk en la carpeta Inicio de Windows',
      archTitle: 'Archivos claros que puedes leer y respaldar.',
      archDesc:
        'Sin bases binarias opacas: inspecciona tus eventos y ajustes en cualquier editor de texto.',
      spec1Title: 'Ajustes locales (agenda.ini)',
      spec1Desc:
        'Carpeta de datos, vista inicial, fuente, audio de aviso y opciones de bandeja junto al ejecutable.',
      spec2Title: 'Historial versionado (versions\\)',
      spec2Desc:
        'Cada cambio incrementa el contador de versión y archiva un snapshot JSON inmutable.',
      spec3Title: 'Identidad nativa de Windows',
      spec3Desc:
        'Incluye metadatos de versión, manifiesto DPI Per-Monitor v2 e icono multi-resolución en el ejecutable.',
      buildTitle: 'Compilar desde el código en Windows.',
      buildDesc:
        'Enlaza únicamente con bibliotecas estándar del sistema Windows incluidas en MinGW-w64.',
      buildStep1Title: '01. Compilar con MinGW',
      buildStep1Desc:
        'Ejecuta mingw32-make.exe en la raíz del proyecto. Los objetos se crean en obj\\ junto con AgendaWin32.exe.',
      buildStep2Title: '02. Publicar en GitHub Pages',
      buildStep2Desc:
        'Este sitio estático se encuentra en la carpeta pages\\. Publica la carpeta /pages desde los ajustes de GitHub Pages o mediante GitHub Actions.',
      copyLabel: 'Copiar',
      copiedLabel: 'Copiado',
      footerText: 'Agenda — Calendario nativo C Win32 Unicode',
      sampleBirthday: 'Marie Dupont (36 años)',
      sampleMeeting: 'Revisión de producto',
      sampleDoctor: 'Dentista',
      sampleTax: 'Primer lunes del mes',
      monthHeader: 'Noviembre 2026',
      yearHeader: 'Año 2026',
      weekHeader: 'Semana del 23 nov. 2026',
      dayHeader: 'Martes 24 de noviembre de 2026',
      dows: ['Lun', 'Mar', 'Mié', 'Jue', 'Vie', 'Sáb', 'Dom'],
      monthsShort: ['Ene', 'Feb', 'Mar', 'Abr', 'May', 'Jun', 'Jul', 'Ago', 'Sep', 'Oct', 'Nov', 'Dic']
    },
    de: {
      pageTitle: 'Agenda — Nativer Windows-Kalender in C Win32',
      pageDesc:
        'Abhängigkeitsfreier nativer Windows-Kalender in reinem C Win32 Unicode mit versionierten JSON-Snapshots, Hell/Dunkel-Systemdesign und Tray-Benachrichtigungen.',
      navFeatures: 'Funktionen',
      navPreview: 'Vorschau',
      navArchitecture: 'Architektur',
      navBuild: 'Kompilieren',
      heroMeta: 'C Win32 Unicode · Keine Abhängigkeiten · MinGW',
      heroTitle: 'Ein schneller, lokaler Kalender nativ für Windows.',
      heroDesc:
        'Geschrieben in reinem C Win32 Unicode ohne Frameworks oder eingebetteten Browser. Bietet Tag-, Wochen-, Monats- und Jahresansicht, flexible Wiederholungen, Geburtstage mit Altersberechnung, Windows-Designanpassung und zeitgestempelte JSON-Versionierung.',
      ctaPrimary: 'Mit MinGW bauen',
      ctaSecondary: 'Funktionen entdecken',
      specLangLabel: 'Quellcode',
      specLangValue: 'C Win32 Unicode',
      specStorageLabel: 'Daten & Einstellungen',
      specStorageValue: 'JSON + agenda.ini',
      specDepLabel: 'Externe Bibliotheken',
      specDepValue: 'Keine',
      previewDarkLabel: 'Dunkles Design',
      previewLightLabel: 'Helles Design',
      tabDay: 'Tag',
      tabWeek: 'Woche',
      tabMonth: 'Monat',
      tabYear: 'Jahr',
      toastApp: 'Agenda',
      toastTitle: '[09:00] Marie Dupont (36 Jahre)',
      toastBody: 'Geburtstag: 36 Jahre — Mittags anrufen',
      featuresTitle: 'Entwickelt für Tempo, Klarheit und volle Datenkontrolle.',
      featuresDesc:
        'Jedes Bedienelement nutzt direkt die Windows-System-APIs – ganz ohne Einträge in der Windows-Registrierung.',
      f1Kicker: '01. Vier Ansichten & Direktbearbeitung',
      f1Title: 'Tag, Woche, Monat und Jahr',
      f1Body:
        'Wechseln Sie zwischen Stundenrastern, Monatsübersicht und 4×3-Jahresansicht. Ein Klick auf einen markierten Tag in der Jahresansicht öffnet direkt den Termin oder das Tagesmenü.',
      f1L1: 'Sofortige Suchfilterung über Titel und Beschreibung',
      f1L2: 'Aktuelle Zeitlinie und parallele Terminspuren',
      f1L3: 'Eigene Themen-Auswahlfelder für Datum, Jahr (Mausrad & Tastatur) und Uhrzeit',
      f2Kicker: '02. Serien & Geburtstage',
      f2Title: 'Automatische Altersberechnung',
      f2Body:
        'Erstellen Sie Einzeltermine oder Wiederholungen nach Tag, Woche, Monatstag, n-tem Wochentag (z. B. erster Montag), Jahr oder im Geburtstagsmodus.',
      f2L1: 'Zeigt das berechnete Alter direkt im Kalender und in Hinweisen',
      f2L2: 'Zuverlässige Behandlung von Schaltjahren (29. Februar)',
      f2L3: 'Separater Jahreswähler für eine schnelle Geburtsjahrauswahl',
      f3Kicker: '03. Transparente Datenhaltung',
      f3Title: 'Lesbares JSON & unbegrenzte Snapshots',
      f3Body:
        'Wählen Sie Ihren eigenen Datenordner. Jede Änderung speichert automatisch einen zeitgestempelten JSON-Snapshot im Unterordner versions\\.',
      f3L1: 'Portable Einstellungen in agenda.ini (keine Registry-Einträge)',
      f3L2: 'Speichert benutzerdefinierte Schriftart, Größe und Ansicht',
      f3L3: 'Einfache Wiederherstellung durch Kopieren eines Snapshots',
      f4Kicker: '04. Tiefe Windows-Integration',
      f4Title: 'Systemdesign, Tray & Erinnerungen',
      f4Body:
        'Passt sich live an den hellen oder dunklen Windows-Modus an, minimiert in den Infobereich und legt optional eine .lnk-Autostart-Verknüpfung an.',
      f4L1: 'Native Windows-Benachrichtigungen mit Beschreibung und eigenem .wav-Ton',
      f4L2: 'Minimierter Start und Minimieren in den Infobereich',
      f4L3: 'Saubere .lnk-Verknüpfung im Windows-Autostart-Ordner',
      archTitle: 'Klartextdateien zum Prüfen und Sichern.',
      archDesc:
        'Keine proprietären Binärdatenbanken: Alle Termine und Einstellungen bleiben in jedem Texteditor lesbar.',
      spec1Title: 'Lokale Konfiguration (agenda.ini)',
      spec1Desc:
        'Datenpfad, Standardansicht, Schriftart, Erinnerungston und Tray-Optionen direkt neben der EXE.',
      spec2Title: 'Versionshistorie (versions\\)',
      spec2Desc:
        'Jede Änderung erhöht den Versionszähler und archiviert einen zeitgestempelten JSON-Snapshot.',
      spec3Title: 'Saubere Windows-Metadaten',
      spec3Desc:
        'Eingebettete Versionsressource, Per-Monitor-DPI-v2-Manifest und mehrstufiges Anwendungssymbol.',
      buildTitle: 'Unter Windows aus den Quellen bauen.',
      buildDesc:
        'Linkt ausschließlich gegen die in MinGW-w64 enthaltenen Windows-Systembibliotheken.',
      buildStep1Title: '01. Mit MinGW kompilieren',
      buildStep1Desc:
        'Führen Sie mingw32-make.exe im Projektverzeichnis aus. Objektdateien landen in obj\\, das Ergebnis ist AgendaWin32.exe.',
      buildStep2Title: '02. Auf GitHub Pages veröffentlichen',
      buildStep2Desc:
        'Diese statische Website liegt im Ordner pages\\. Aktivieren Sie GitHub Pages für /pages oder nutzen Sie GitHub Actions.',
      copyLabel: 'Kopieren',
      copiedLabel: 'Kopiert',
      footerText: 'Agenda — Nativer C-Win32-Unicode-Kalender',
      sampleBirthday: 'Marie Dupont (36 Jahre)',
      sampleMeeting: 'Produkt-Review',
      sampleDoctor: 'Zahnarzt',
      sampleTax: 'Erster Montag im Monat',
      monthHeader: 'November 2026',
      yearHeader: 'Jahr 2026',
      weekHeader: 'Woche vom 23. Nov. 2026',
      dayHeader: 'Dienstag, 24. November 2026',
      dows: ['Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa', 'So'],
      monthsShort: ['Jan', 'Feb', 'Mär', 'Apr', 'Mai', 'Jun', 'Jul', 'Aug', 'Sep', 'Okt', 'Nov', 'Dez']
    }
  };

  var CODE_SNIPPETS = {
    json:
      '{\n' +
      '  "format": "AgendaWin32Events",\n' +
      '  "version": 14,\n' +
      '  "events": [\n' +
      '    {\n' +
      '      "id": 1,\n' +
      '      "title": "Marie Dupont",\n' +
      '      "description": "Appeler a midi",\n' +
      '      "start": "1990-11-24T09:00:00",\n' +
      '      "durationMinutes": 60,\n' +
      '      "allDay": false,\n' +
      '      "recurrenceType": 6,\n' +
      '      "recurrenceInterval": 1,\n' +
      '      "notifyMode": 2,\n' +
      '      "notifyMinutesBefore": 15,\n' +
      '      "colorIndex": 6\n' +
      '    }\n' +
      '  ]\n' +
      '}',
    ini:
      '[General]\n' +
      'DataFolder=C:\\Users\\User\\Documents\\AgendaData\n' +
      'ViewMode=2\n' +
      'FontName=Segoe UI\n' +
      'FontSizePt=9\n\n' +
      '[Tray]\n' +
      'StartMinimized=1\n' +
      'MinimizeToTray=1\n\n' +
      '[Notifications]\n' +
      'SoundEnabled=1\n' +
      'SoundPath=\n\n' +
      '[System]\n' +
      'StartWithWindows=0',
    makefile:
      'CC     := gcc\n' +
      'CFLAGS := -Wall -Wextra -Wshadow -O2 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00\n' +
      'LIBS   := -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lwinmm -lshlwapi\n\n' +
      '$(TARGET): $(OBJS) $(RES_OBJ)\n' +
      '\t$(CC) -s -municode -mwindows -o $@ $^ $(LIBS)'
  };

  var currentLang = 'fr';
  var currentView = 'month';
  var currentCodeTab = 'json';
  var isPreviewDark = false;

  function detectInitialLang() {
    var params = new URLSearchParams(window.location.search);
    var urlLang = (params.get('lang') || '').toLowerCase().slice(0, 2);
    if (TRANSLATIONS[urlLang]) return urlLang;
    try {
      var saved = (localStorage.getItem('agenda_site_lang') || '').toLowerCase();
      if (TRANSLATIONS[saved]) return saved;
    } catch (e) {
      /* ignore storage errors */
    }
    var nav = (navigator.language || 'fr').toLowerCase().slice(0, 2);
    return TRANSLATIONS[nav] ? nav : 'fr';
  }

  function setLanguage(lang) {
    if (!TRANSLATIONS[lang]) lang = 'fr';
    currentLang = lang;
    var dict = TRANSLATIONS[lang];

    document.documentElement.lang = lang;
    document.title = dict.pageTitle;
    var metaDesc = document.querySelector('meta[name="description"]');
    if (metaDesc) metaDesc.setAttribute('content', dict.pageDesc);

    var nodes = document.querySelectorAll('[data-i18n]');
    for (var i = 0; i < nodes.length; i++) {
      var key = nodes[i].getAttribute('data-i18n');
      if (dict[key] !== undefined) {
        nodes[i].textContent = dict[key];
      }
    }

    var langBtns = document.querySelectorAll('.lang-btn');
    for (var j = 0; j < langBtns.length; j++) {
      var bLang = langBtns[j].getAttribute('data-lang');
      langBtns[j].setAttribute('aria-pressed', bLang === lang ? 'true' : 'false');
    }

    var themeBtn = document.getElementById('previewThemeToggle');
    if (themeBtn) {
      themeBtn.textContent = isPreviewDark ? dict.previewLightLabel : dict.previewDarkLabel;
    }

    try {
      localStorage.setItem('agenda_site_lang', lang);
    } catch (e) {
      /* ignore */
    }

    try {
      var url = new URL(window.location.href);
      url.searchParams.set('lang', lang);
      window.history.replaceState(null, '', url.toString());
    } catch (e) {
      /* ignore */
    }

    renderPreviewCanvas();
  }

  function renderPreviewCanvas() {
    var dict = TRANSLATIONS[currentLang];
    var periodEl = document.getElementById('previewPeriod');
    var canvasEl = document.getElementById('previewCanvasBody');
    if (!canvasEl || !periodEl) return;

    var tabBtns = document.querySelectorAll('.win-tab');
    for (var t = 0; t < tabBtns.length; t++) {
      var v = tabBtns[t].getAttribute('data-view');
      tabBtns[t].setAttribute('aria-selected', v === currentView ? 'true' : 'false');
    }

    if (currentView === 'month') {
      periodEl.textContent = dict.monthHeader;
      var html = '<div class="cal-month-grid">';
      for (var d = 0; d < 7; d++) {
        html += '<div class="cal-dow">' + dict.dows[d] + '</div>';
      }
      var days = [
        { n: 26, m: true }, { n: 27, m: true }, { n: 28, m: true }, { n: 29, m: true },
        { n: 30, m: true }, { n: 31, m: true }, { n: 1, m: false },
        { n: 2, m: false, ev: { cls: 'orange', text: '10:00 ' + dict.sampleTax } },
        { n: 3, m: false }, { n: 4, m: false }, { n: 5, m: false }, { n: 6, m: false },
        { n: 7, m: false }, { n: 8, m: false }, { n: 9, m: false }, { n: 10, m: false },
        { n: 11, m: false, ev: { cls: 'green', text: '14:30 ' + dict.sampleDoctor } },
        { n: 12, m: false }, { n: 13, m: false }, { n: 14, m: false }, { n: 15, m: false },
        { n: 16, m: false }, { n: 17, m: false }, { n: 18, m: false },
        { n: 19, m: false, ev: { cls: 'blue', text: '11:00 ' + dict.sampleMeeting } },
        { n: 20, m: false }, { n: 21, m: false }, { n: 22, m: false },
        { n: 23, m: false },
        { n: 24, m: false, today: true, ev: { cls: 'rose', text: '09:00 ' + dict.sampleBirthday } },
        { n: 25, m: false }, { n: 26, m: false }, { n: 27, m: false }, { n: 28, m: false }, { n: 29, m: false }
      ];
      for (var i = 0; i < days.length; i++) {
        var cell = days[i];
        html += '<div class="cal-cell' + (cell.today ? ' is-today' : '') + '">';
        html += '<span class="cal-day-num' + (cell.m ? ' is-muted' : '') + '">' + cell.n + '</span>';
        if (cell.ev) {
          html += '<span class="cal-ev ' + cell.ev.cls + '">' + cell.ev.text + '</span>';
        }
        html += '</div>';
      }
      html += '</div>';
      canvasEl.innerHTML = html;
    } else if (currentView === 'year') {
      periodEl.textContent = dict.yearHeader;
      var yHtml = '<div class="cal-year-grid">';
      for (var m = 0; m < 12; m++) {
        yHtml += '<div class="mini-month"><div class="mini-month-name">' + dict.monthsShort[m] + '</div><div class="mini-dots">';
        for (var dn = 1; dn <= 28; dn++) {
          var cls = '';
          if (m === 10 && dn === 24) cls = 'mini-dot-today';
          else if ((m === 10 && (dn === 2 || dn === 11 || dn === 19)) || (m === 4 && dn === 14)) cls = 'mini-dot-active';
          yHtml += '<span class="' + cls + '">' + dn + '</span>';
        }
        yHtml += '</div></div>';
      }
      yHtml += '</div>';
      canvasEl.innerHTML = yHtml;
    } else if (currentView === 'week') {
      periodEl.textContent = dict.weekHeader;
      canvasEl.innerHTML =
        '<div class="cal-timeline">' +
        '<div class="timeline-row"><span class="timeline-hour">' + dict.dows[0] + ' 23</span><span class="cal-ev orange">10:00 ' + dict.sampleTax + '</span></div>' +
        '<div class="timeline-row"><span class="timeline-hour">' + dict.dows[1] + ' 24</span><span class="cal-ev rose">09:00 ' + dict.sampleBirthday + '</span></div>' +
        '<div class="timeline-row"><span class="timeline-hour">' + dict.dows[3] + ' 26</span><span class="cal-ev blue">11:00 ' + dict.sampleMeeting + '</span></div>' +
        '<div class="timeline-row"><span class="timeline-hour">' + dict.dows[4] + ' 27</span><span class="cal-ev green">14:30 ' + dict.sampleDoctor + '</span></div>' +
        '</div>';
    } else {
      periodEl.textContent = dict.dayHeader;
      canvasEl.innerHTML =
        '<div class="cal-timeline">' +
        '<div class="timeline-row"><span class="timeline-hour">09:00</span><span class="cal-ev rose">' + dict.sampleBirthday + '</span></div>' +
        '<div class="timeline-row"><span class="timeline-hour">11:00</span><span class="cal-ev blue">' + dict.sampleMeeting + '</span></div>' +
        '<div class="timeline-row"><span class="timeline-hour">14:30</span><span class="cal-ev green">' + dict.sampleDoctor + '</span></div>' +
        '<div class="timeline-row"><span class="timeline-hour">17:00</span><span class="cal-ev orange">' + dict.sampleTax + '</span></div>' +
        '</div>';
    }
  }

  function renderCodeSnippet() {
    var pre = document.getElementById('codeSnippetPre');
    if (pre && CODE_SNIPPETS[currentCodeTab]) {
      pre.textContent = CODE_SNIPPETS[currentCodeTab];
    }
    var tabs = document.querySelectorAll('.code-tab');
    for (var i = 0; i < tabs.length; i++) {
      var key = tabs[i].getAttribute('data-code');
      tabs[i].setAttribute('aria-selected', key === currentCodeTab ? 'true' : 'false');
    }
  }

  function attachCopyHandler(btn, getTextFn) {
    if (!btn) return;
    btn.addEventListener('click', function () {
      var text = getTextFn();
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(function () {
          showCopied(btn);
        });
      } else {
        var ta = document.createElement('textarea');
        ta.value = text;
        ta.setAttribute('readonly', '');
        ta.style.position = 'absolute';
        ta.style.left = '-9999px';
        document.body.appendChild(ta);
        ta.select();
        document.execCommand('copy');
        document.body.removeChild(ta);
        showCopied(btn);
      }
    });
  }

  function showCopied(btn) {
    var dict = TRANSLATIONS[currentLang];
    btn.textContent = dict.copiedLabel;
    window.setTimeout(function () {
      btn.textContent = TRANSLATIONS[currentLang].copyLabel;
    }, 1500);
  }

  document.addEventListener('DOMContentLoaded', function () {
    if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
      isPreviewDark = true;
      var shell = document.getElementById('appPreviewShell');
      if (shell) shell.classList.add('preview-dark');
    }

    var langBtns = document.querySelectorAll('.lang-btn');
    for (var i = 0; i < langBtns.length; i++) {
      langBtns[i].addEventListener('click', function () {
        setLanguage(this.getAttribute('data-lang'));
      });
    }

    var viewBtns = document.querySelectorAll('.win-tab');
    for (var j = 0; j < viewBtns.length; j++) {
      viewBtns[j].addEventListener('click', function () {
        currentView = this.getAttribute('data-view');
        renderPreviewCanvas();
      });
    }

    var themeBtn = document.getElementById('previewThemeToggle');
    if (themeBtn) {
      themeBtn.addEventListener('click', function () {
        isPreviewDark = !isPreviewDark;
        var shell = document.getElementById('appPreviewShell');
        if (shell) {
          shell.classList.toggle('preview-dark', isPreviewDark);
        }
        themeBtn.textContent = isPreviewDark
          ? TRANSLATIONS[currentLang].previewLightLabel
          : TRANSLATIONS[currentLang].previewDarkLabel;
      });
    }

    var codeTabs = document.querySelectorAll('.code-tab');
    for (var k = 0; k < codeTabs.length; k++) {
      codeTabs[k].addEventListener('click', function () {
        currentCodeTab = this.getAttribute('data-code');
        renderCodeSnippet();
      });
    }

    attachCopyHandler(document.getElementById('copyCodeBtn'), function () {
      return CODE_SNIPPETS[currentCodeTab] || '';
    });
    attachCopyHandler(document.getElementById('copyBuildCmdBtn'), function () {
      return 'mingw32-make.exe';
    });

    setLanguage(detectInitialLang());
    renderCodeSnippet();
  });
})();
