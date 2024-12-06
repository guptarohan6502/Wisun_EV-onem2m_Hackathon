******EV Charger Monitoring Dashboard******

This web-based dashboard provides monitoring capabilities for EV chargers. It features user and admin roles, each with specific functionalities, and includes user-friendly features such as filtering, history tracking, and user authentication.

****Features****

**User Role:**

Monitor charging history with parameters such as:

            Charger ID
            
            Amount charged
            
            Time of charging

**Admin Role:**

Monitor charger details such as:

            Coordinates of the chargers
            
            Charger health status

**Common Features:**

            User authentication and role-based access.
            
            Filtering options for efficient data management.
            
            Interactive map integration using Leaflet for charger locations.

Analytical insights for better data understanding:

                        Graphical representation of charging trends over time.
                        
                        Summary statistics, such as total charges and average session durations.
                        
                        Charger health analytics for predictive maintenance.

****Setup Instructions****

Follow these steps to set up the EV Charger Monitoring Dashboard on your local machine.

**Clone the Repository**

First, clone the project repository from GitHub:

git clone https://github.com/perireddyvaka/evcharger.git

****Backend Setup****

Navigate to the backend directory (if applicable) or work in the main project directory.

Install the required Python libraries and modules:

            pip install fastapi uvicorn requests

Start the FastAPI backend server by running:

            python main.py

**Frontend Setup**

1. Navigate to the evcharger directory:

            cd evcharger

2. Install the required React scripts and dependencies:

            npm install react-scripts @mui/material react-dom leaflet

3. Start the React frontend application:

            npm start

****Usage****
1. Once the backend and frontend are running, open the web application in your browser (default: http://localhost:3000).

2. Login using your credentials:

            User role: Access your charging history.
            
            Admin role: Monitor charger coordinates and health.

3. Explore filtering options, graphical analytics, and other features as per your role.

****Technologies Used****

            Frontend: React, Material-UI, Leaflet
                        
            Backend: FastAPI, Python
                        
            Database: OM2M (for EV charger data storage)

**Contribution**

Feel free to fork the repository and create pull requests for any improvements or new features. For major changes, please open an issue to discuss what you would like to change.

**License**

This project is licensed under the MIT License. See the LICENSE file for details.

**Support**

For any issues or questions, please contact https://github.com/perireddyvaka/evcharger or raise an issue in the GitHub repository.

