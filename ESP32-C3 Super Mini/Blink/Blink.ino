const int ledPin = 8; 

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
}

void loop() {
  digitalWrite(ledPin, HIGH); 
  Serial.println("LED OFF");
  delay(2500);                     
  digitalWrite(ledPin, LOW); 
  Serial.println("LED ON");  
  delay(2500);                     
}