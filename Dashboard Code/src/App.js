import React from 'react';
import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
import './App.css';
import Login from './components/login';
import User from './components/user';
import Charger from './components/charger';

function App() {
  return (
    <Router>
      <div className="App">
        <Routes>
          <Route path="/" element={<Login />} />
          <Route path="/user" element={<User />} />
          <Route path="/charger" element={<Charger />} />
        </Routes>
      </div>
    </Router>
  );
}

export default App;