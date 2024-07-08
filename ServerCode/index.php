<?php
function decrypt($data) {
    $shift = 3; 
    $decrypted = "";
    for ($i = 0; $i < strlen($data); $i++) {
        $decrypted .= chr(ord($data[$i]) - $shift);
    }
    return $decrypted;
}

if ($_SERVER["REQUEST_METHOD"] == "POST") {
    $encryptedData = file_get_contents('php://input');
    $data = decrypt($encryptedData);

    parse_str($data, $output);

    $device = $output['device'];
    $temperature = $output['temperature'];
    $humidity = $output['humidity'];
    $co2 = $output['co2'];
    $heart_rate = $output['heart_rate'];
    $oxygen = $output['oxygen'];
    $pressure = $output['pressure'];
    $air_quality = $output['air_quality'];

    // Get current date and time
    date_default_timezone_set('Asia/Tehran'); // Set timezone to Tehran
    $datetime = date("Y-m-d H:i:s");
    
    // Numbering messages
    $file = 'data.txt';
    $currentData = file_get_contents($file);
    $lines = explode("\n", trim($currentData));
    $messageNumber = count($lines) + 1;

    // Save data to text file
    $currentData .= "$messageNumber, $datetime, Device: $device, Temperature: $temperature, Humidity: $humidity, CO2: $co2, Heart Rate: $heart_rate, Oxygen: $oxygen, Pressure: $pressure, Air Quality: $air_quality, Encrypted Message: $encryptedData\n";
    file_put_contents($file, $currentData);

    // Save data to Excel file
    $fileExcel = 'data.csv';
    $currentDataExcel = file_get_contents($fileExcel);
    $currentDataExcel .= "$messageNumber, $datetime, $device, $temperature, $humidity, $co2, $heart_rate, $oxygen, $pressure, $air_quality, $encryptedData\n";
    file_put_contents($fileExcel, $currentDataExcel);

    // Response to ESP32 request
    echo "Data received: Message Number: $messageNumber, Datetime: $datetime, Device: $device, Temperature: $temperature, Humidity: $humidity, CO2: $co2, Heart Rate: $heart_rate, Oxygen: $oxygen, Pressure: $pressure, Air Quality: $air_quality, Encrypted Message: $encryptedData";
    exit;
}
?>
