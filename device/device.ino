const int beepbeep = 2; // i'm a sheep

const int echoOne = 8;
const int trigOne = 9;
const int echoTwo = 10;
const int trigTwo = 11;

const int acceptable_min = 5; // maximum distance in cm to be considered in range
const int in_range_up_to = 30; // distance in cm from which alerts are disabled

void setup() {
  pinMode(beepbeep, OUTPUT);
  pinMode(echoOne, INPUT);
  pinMode(trigOne, OUTPUT);
  pinMode(echoTwo, INPUT);
  pinMode(trigTwo, OUTPUT);
  Serial.begin(115200);
}

void ringBeep() {
  digitalWrite(beepbeep, HIGH);
  delay(80);
  digitalWrite(beepbeep, LOW);
}

void loop() {
  bool good_position = true;

  //// GET DISTANCES \\\\
  
  digitalWrite(trigOne, LOW);
  digitalWrite(trigTwo, LOW);
  delayMicroseconds(2);

  // trigger ultrasonic sensor 1
  digitalWrite(trigOne, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigOne, LOW);

  // read travel time 1
  long durationOne = pulseIn(echoOne, HIGH);

  // trigger ultrasonic sensor 2
  digitalWrite(trigTwo, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigTwo, LOW);

  // read travel time 2
  long durationTwo = pulseIn(echoTwo, HIGH);
  
  // calculate distance in cm
  int distanceOne = durationOne * 0.034/2;
  int distanceTwo = durationTwo * 0.034/2;

  //// END GET DISTANCES \\\\


  //// DISTANCE LOGIC \\\\

  bool in_range = distanceOne <= in_range_up_to && distanceTwo <= in_range_up_to;
  if (in_range) {
    good_position = distanceOne <= acceptable_min && distanceTwo <= acceptable_min;
    if (!good_position) {
      ringBeep();
    }
  }

  //// END DISTANCE LOGIC \\\\

  //// DEBUG PRINTING \\\\

  Serial.print("Distance one ");
  Serial.print(distanceOne);
  Serial.print(", two ");
  Serial.print(distanceTwo);
  if (in_range) {
    Serial.print("; good position: ");
    Serial.println(good_position ? "yes" : "no");
  } else {
    Serial.println("; out of range");
  }

  //// END DEBUG PRINTING \\\\

  delay(600);
}
