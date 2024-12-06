import React from 'react';
import ReactApexChart from 'react-apexcharts';
import AppBar from './appbar'; // Import the AppBar component
import axios from 'axios'; // Import axios for API calls
import moment from 'moment-timezone'; // Import moment-timezone for date manipulation
import { MapContainer, TileLayer, Marker, Popup } from 'react-leaflet'; // Leaflet components
import L from 'leaflet'; // Leaflet library
import 'leaflet/dist/leaflet.css'; // Leaflet CSS

// Import custom marker image
import markerImage from './image.png'; // Adjust this path based on where the image is stored

class ChargerDashboard extends React.Component {
  constructor(props) {
    super(props);

    this.state = {
      charger1Series: [], // To hold the fetched data for Charger 1
      charger2Series: [
        { name: 'Charger 2', data: this.generateDummyData() }, // Dummy data for Charger 2
      ],
      options: {
        chart: {
          height: 350,
          type: 'bar',
          background: '#333',
        },
        plotOptions: {
          bar: {
            borderRadius: 10,
            dataLabels: {
              position: 'top',
            },
          },
        },
        dataLabels: {
          enabled: true,
          formatter: function (val) {
            return val + " kWh";
          },
          offsetY: -20,
          style: {
            fontSize: '12px',
            colors: ["#ffffff"], // Set data label color to white
          },
        },
        xaxis: {
          categories: [], // To be filled with timestamps in IST
          position: 'top',
          axisBorder: {
            show: false,
          },
          axisTicks: {
            show: false,
          },
          labels: {
            style: {
              colors: '#ffffff', // Set x-axis labels color to white
            },
          },
        },
        yaxis: {
          axisBorder: {
            show: false,
          },
          axisTicks: {
            show: false,
          },
          labels: {
            show: true,
            formatter: function (val) {
              return val + " kWh";
            },
            style: {
              colors: '#ffffff', // Set y-axis labels color to white
            },
          },
        },
        title: {
          text: 'Monthly Energy Consumption for Chargers',
          floating: true,
          offsetY: 330,
          align: 'center',
          style: {
            color: '#ffffff', // Set title color to white
          },
        },
        tooltip: {
          theme: 'dark',
          style: {
            fontSize: '14px',
            color: '#000000', // Set tooltip text color to black
          },
        },
      },
      positions: [
        [17.445289, 78.349593],
        [17.445160, 78.349806],
      ], // Provided coordinates
    };
  }

  componentDidMount() {
    // Fetch data for Charger 1
    this.fetchChargerData();
  }

  fetchChargerData = async () => {
    try {
      const response = await axios.get("http://localhost:8000/charger/get-transactions");
      const transactions = response.data.transactions || []; // Assuming the data has a 'transactions' field

      // Process the fetched data
      const charger1Data = transactions.map(item => ({
        x: moment.unix(item.Timestamp).tz('Asia/Kolkata').format('YYYY-MM-DD HH:mm:ss'), // Convert to IST
        y: item.Units_consumed, // Units consumed
      }));

      // Set the state with fetched data for Charger 1
      this.setState({
        charger1Series: [
          { name: 'Charger 1', data: charger1Data }, // Update Charger 1 data
        ],
        options: {
          ...this.state.options,
          xaxis: {
            ...this.state.options.xaxis,
            categories: charger1Data.map(item => item.x), // Set x-axis categories as time in IST
          },
        },
      });
    } catch (error) {
      console.error("Error fetching charger data:", error);
    }
  };

  generateDummyData = () => {
    const dummyData = [];
    for (let i = 1; i <= 12; i++) {
      dummyData.push({
        x: `2024-${String(i).padStart(2, '0')}-01`, // Generate random date (dummy data)
        y: Math.floor(Math.random() * 20) + 5, // Random units consumed between 5 and 25
      });
    }
    return dummyData;
  };

  render() {
    return (
      <div style={{ backgroundColor: '#121212', color: '#fff', minHeight: '100vh', padding: '20px' }}>
        <AppBar />
        
        {/* Map Section */}
        <div style={{ height: '45vh', marginTop: '1vw' }}>
          <MapContainer center={[17.445289, 78.349593]} zoom={17} style={{ width: '100%', height: '100%' }}>
            <TileLayer
              url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
            />
            {this.state.positions.map((position, index) => (
              <Marker
                key={index}
                position={position} // Set position from state
                icon={new L.Icon({
                  iconUrl: markerImage, // Use the custom image as the marker icon
                  iconSize: [70, 70], // Adjust the size of the marker icon as needed
                  iconAnchor: [15, 30], // Adjust the anchor point to position it correctly
                  popupAnchor: [0, -30], // Adjust the popup anchor if needed
                })}
              >
                <Popup>{`Charger ${index + 1}`}</Popup>
              </Marker>
            ))}
          </MapContainer>
        </div>

        {/* Charger Energy Consumption Section */}
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
          <div style={{ flex: 1, marginRight: '20px' }}>
            <h3>Charger 1 Energy Consumption</h3>
            <ReactApexChart
              options={this.state.options}
              series={this.state.charger1Series}
              type="bar"
              height={350}
            />
            
          </div>
          

          <div style={{ flex: 1 }}>
            <h3>Charger 2 Energy Consumption</h3>
            <ReactApexChart
              options={this.state.options}
              series={this.state.charger2Series}
              type="bar"
              height={350}
            />
          </div>
        </div>
      </div>
    );
  }
}

export default ChargerDashboard;
