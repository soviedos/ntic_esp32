/**
 * @Author: Sergio Oviedo Seas
 * @Date:   2021-03-07 19:37:40
 * @Last Modified by:   Sergio Oviedo Seas
 * @Last Modified time: 2021-03-27 19:44:29
 */

#define IOT_HUB_NAME_NTic "NTicSolution"
#define DEVICE_NAME_NTic "esp32"
#define SAS_TOKEN_NTic "SharedAccessSignature sr=NTicSolution.azure-devices.net%2Fdevices%2Fesp32&sig=Efmz0F7r9zocWE97Y4IYMDdEuLw2tl6sJUN9TE0eTxo%3D&se=1648039565"

#define SSID_SECRET "TNT1"     // your network SSID (name of wifi network)
#define PASS_SECRET "DFGHMNBVZX" // your network password
#define ROOT_CERT \
     "-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n"