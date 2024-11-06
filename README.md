The smart access control system captures facial images with the ESP32-CAM 
and sends them to a Python server for processing. The server assesses the image 
to determine access status and updates Firebase with the result. Every access 
event, whether granted or denied, triggers an email alert to the administrator 
through EmailJS. Additionally, the system activates a servo motor to control 
physical access if granted. The user interface (UI) provides functionality to 
register new faces and view detailed logs of all access events. This log feature 
allows users to review historical access data, ensuring effective monitoring and 
management of access control activities
