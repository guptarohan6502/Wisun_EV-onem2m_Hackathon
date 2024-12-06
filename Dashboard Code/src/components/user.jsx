import React, { useEffect, useState } from 'react';
import AppBar from './appbar';
import scrclogo from './scrclogo.png';

const UserDashboard = () => {
  const [transactions, setTransactions] = useState([]);

  useEffect(() => {
    const fetchTransactions = async () => {
      try {
        const response = await fetch('http://localhost:8000/user/get-transactions');
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        const data = await response.json();
        if (data.transactions) {
          const mappedTransactions = data.transactions.map(transaction => ({
            charger: transaction.Charger_id,
            amount: transaction.Transaction_Amount,
            time: new Date(transaction.Timestamp * 1000).toLocaleString('en-IN', {
              year: 'numeric',
              month: '2-digit',
              day: '2-digit',
              hour: '2-digit',
              minute: '2-digit',
              second: '2-digit',
              hour12: false,
            }),
          }));
          setTransactions(mappedTransactions);
        } else {
          console.error('Error: No transactions found in the response');
        }
      } catch (error) {
        console.error('Error fetching transactions:', error);
      }
    };

    fetchTransactions();
  }, []);

  return (
    <div style={{ padding: '20px', fontFamily: 'Arial, sans-serif', display: 'flex', flexDirection: 'column', height: '100vh', backgroundColor: '#121212', color: '#f0f0f0', boxSizing: 'border-box' }}>
      <div style={{ marginBottom: '20px' }}>
        <AppBar />
      </div>

      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px', flex: '1' }}>
        <div style={{ flex: '0 0 150px', textAlign: 'center' }}>
          <img src={scrclogo} alt="Profile" style={{ borderRadius: '50%', width: '150px', height: '150px' }} />
        </div>
        <div style={{ flex: '1', textAlign: 'right', paddingRight: '20px' }}>
          <h2 style={{ margin: '0', fontSize: '24px' }}>Name: John Doe</h2>
          <p style={{ margin: '5px 0', fontSize: '18px' }}>User ID: 123456</p>
        </div>
      </div>

      <div style={{ flex: '2', marginTop: '20px', overflowY: 'auto' }}>
        <h3 style={{ fontSize: '20px', marginBottom: '10px' }}>Last Transactions</h3>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr>
              <th style={{ border: '1px solid #444', padding: '10px', textAlign: 'left', backgroundColor: '#333', color: '#fff' }}>Charger</th>
              <th style={{ border: '1px solid #444', padding: '10px', textAlign: 'left', backgroundColor: '#333', color: '#fff' }}>Amount</th>
              <th style={{ border: '1px solid #444', padding: '10px', textAlign: 'left', backgroundColor: '#333', color: '#fff' }}>Time</th>
            </tr>
          </thead>
          <tbody>
            {transactions.map((transaction, index) => (
              <tr key={index}>
                <td style={{ border: '1px solid #444', padding: '10px' }}>{transaction.charger}</td>
                <td style={{ border: '1px solid #444', padding: '10px' }}>{transaction.amount}</td>
                <td style={{ border: '1px solid #444', padding: '10px' }}>{transaction.time}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default UserDashboard;
