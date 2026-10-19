# v5 storage fixture

`settings-v5.bin` is a complete 17,328-byte v5 StoreRecord produced using the v0.7.0-alpha layout (commit `3c635105a285196d9912f051d7709867525f0d0e`). Sequence 93; calibration zeros 101/202, codes per volt 2010/2020, brightness 42. Its CRC covers the original bytes, including the zeroed CRC field during calculation.

The current reader validates the original record and preserves calibrated hardware values while replacing musical settings. The test also corrupts a byte to verify CRC rejection. Do not regenerate this fixture using the current structs: retaining the old binary layout is the point of this test.
