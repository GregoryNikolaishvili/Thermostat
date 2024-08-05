const byte DEF_SETTINGS_VERSION_BOILER = 0x01;

static void readSettings() {
  boilerSettings.Mode = 'W';
  boilerSettings.CollectorSwitchOnTempDiff = 80;   // 2...20
  boilerSettings.CollectorSwitchOffTempDiff = 40;  // 0 ... EMOF - 2

  boilerSettings.CollectorEmergencySwitchOffT = 1400;  // EMOF, 110...200
  boilerSettings.CollectorEmergencySwitchOnT = 1200;   // EMON, 0...EMOF - 3

  boilerSettings.CollectorMinimumSwitchOnT = 100;  // CMN, -10...90
  boilerSettings.CollectorAntifreezeT = 40;        // CFR, -10...10
  boilerSettings.MaxTankT = 600;                   // SMX, Max 95
  boilerSettings.AbsoluteMaxTankT = 950;           // 95

  boilerSettings.PoolSwitchOnT = 550;
  boilerSettings.PoolSwitchOffT = 500;

  boilerSettings.BackupHeatingTS1_Start = 400;       // 4am
  boilerSettings.BackupHeatingTS1_End = 500;         // 5am
  boilerSettings.BackupHeatingTS1_SwitchOnT = 400;   // 10 - OFF -> 2
  boilerSettings.BackupHeatingTS1_SwitchOffT = 450;  // ON + 2 -> 80

  boilerSettings.BackupHeatingTS2_Start = 800;  // 8am
  boilerSettings.BackupHeatingTS2_End = 1200;   // 2pm
  boilerSettings.BackupHeatingTS2_SwitchOnT = 500;
  boilerSettings.BackupHeatingTS2_SwitchOffT = 550;

  boilerSettings.BackupHeatingTS3_Start = 1700;  // 5pm
  boilerSettings.BackupHeatingTS3_End = 2300;    // 11pm
  boilerSettings.BackupHeatingTS3_SwitchOnT = 500;
  boilerSettings.BackupHeatingTS3_SwitchOffT = 550;

  // boiler
  byte v = eeprom_read_byte((uint8_t*)STORAGE_ADDRESS_BOILER_SETTINGS);

  if (v != DEF_SETTINGS_VERSION_BOILER) {
    eeprom_update_byte((uint8_t*)STORAGE_ADDRESS_BOILER_SETTINGS, DEF_SETTINGS_VERSION_BOILER);
    saveBoilerSettings(false);
  } else {
    eeprom_read_block((void*)&boilerSettings, (void*)(STORAGE_ADDRESS_BOILER_SETTINGS + 1), sizeof(boilerSettings));
    boilerSettingsChanged(false);
  }
}

static void saveBoilerSettings(bool publish) {
  eeprom_update_block((const void*)&boilerSettings, (void*)(STORAGE_ADDRESS_BOILER_SETTINGS + 1), sizeof(boilerSettings));

  boilerSettingsChanged(publish);
}

static void boilerSettingsChanged(bool publish) {
}
