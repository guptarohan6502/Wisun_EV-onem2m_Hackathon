from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from user import user_app  # Import user app
from charger import charger_app  # Import charger app

# Create the main FastAPI app
app = FastAPI()

# Configure CORS middleware to allow cross-origin requests from all domains (or specify your frontend domain)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # You can replace "*" with your frontend URL (e.g., ["http://localhost:3000"])
    allow_credentials=True,
    allow_methods=["*"],  # Allow all HTTP methods
    allow_headers=["*"],  # Allow all headers
)

# Mount the user and charger FastAPI apps
app.mount("/user", user_app)  # All /user endpoints will be routed to the user app
app.mount("/charger", charger_app)  # All /charger endpoints will be routed to the charger app

# Optional: Main root endpoint
@app.get("/")
def read_root():
    return {"message": "Welcome to the EV Charging System API"}
