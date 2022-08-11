# Delivery-Bot

Delivery Bot at its core is a line following robot. It uses the ATmega32U4 MCU from the Romi 32U4 control board to control the motors for the wheels, an ultrasonic sensor for obstacle detection, and a line sensor for line following. An ESP32 MCU was used to host a webserver where the user can send commands through an internet connected device that will essentially, give path instructions to the robot for pick up and delivery. 

# The Goal

The goal of programming and assembling this robot was to have it pick up cans from one location on a track, and 'deliver' the can at a pre-defined delivery spot. The robot will have to pick up and deliver a total of three cans per run on the following track:

![IMG_0121 2](https://user-images.githubusercontent.com/92708446/184165698-b5494f28-0964-451c-b967-68fa4cdf44df.jpg)


Here, the squares annoted with A, B, C... are delivery spots, and there is one pick up spot. The robot must begin on the starting line and pick up a can from the pick up zone and deliver it to its respective delivery spot. The Romi has all the delivery spots hard coded, which means it knows what direction to drive in to get to the delivery spot it needs to get to. 

# UI

The ESP32 connects to WiFi and hosts a webserver that displays a user interface in order to have the robot fulfil the task. With any internet connected device, the user is able to connect to the ESP32 by typing its IP address on the address bar. When the connection is successful, the following page loads: 

![IMG_1CD0996CE798-1](https://user-images.githubusercontent.com/92708446/184167410-3e9edc6d-cdf9-43ff-a2a3-fd505702fc92.jpeg)

The user is then able to pick how many deliveries the robot will make. If three runs are picked, the user then types in the delivery spots in the text field. For example, 'aaa' will correspond to three deliveries, all at delivery spot A. Once the submit button is pressed, the ESP32 will recieve a message from the webserver that contains how many deliveries the robot must make and to which spots to deliver to. The ESP32 then deciphers the message and sends the delivery spots to the Romi through UART. The Romi receives the delivery spots and begins excecuting them. 
