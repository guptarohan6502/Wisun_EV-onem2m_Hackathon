import * as React from 'react';
import { AppProvider } from '@toolpad/core/AppProvider';
import { SignInPage } from '@toolpad/core/SignInPage';
import { useTheme } from '@mui/material/styles';
import { useNavigate } from 'react-router-dom';

const providers = [{ id: 'credentials', name: 'Email and password' }];

// Predefined credentials
const predefinedCredentials = {
  user: { email: 'user@example.com', password: 'user123' },
  admin: { email: 'admin@example.com', password: 'admin123' },
};

const signIn = async (provider, formData, navigate) => {
  return new Promise((resolve) => {
    setTimeout(() => {
      const email = formData?.get('email');
      const password = formData?.get('password');

      if (
        email === predefinedCredentials.user.email &&
        password === predefinedCredentials.user.password
      ) {
        alert('User signed in successfully! Redirecting to User Page.');
        navigate('/user');
        resolve({ type: 'CredentialsSignin', error: null });
      } else if (
        email === predefinedCredentials.admin.email &&
        password === predefinedCredentials.admin.password
      ) {
        alert('Admin signed in successfully! Redirecting to Charger Page.');
        navigate('/charger');
        resolve({ type: 'CredentialsSignin', error: null });
      } else {
        alert('Invalid credentials. Please try again.');
        resolve({
          type: 'CredentialsSignin',
          error: 'Invalid credentials.',
        });
      }
    }, 300);
  });
};

export default function NotificationsSignInPageError() {
  const theme = useTheme();
  const navigate = useNavigate();

  return (
    <AppProvider theme={theme}>
      <SignInPage
        signIn={(provider, formData) => signIn(provider, formData, navigate)}
        providers={providers}
      />
    </AppProvider>
  );
}

// Add Routes for navigation
// In your main application file (e.g., App.js), include the following routes:

// import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
// import NotificationsSignInPageError from './NotificationsSignInPageError';
// import UserPage from './UserPage';
// import ChargerPage from './ChargerPage';

// function App() {
//   return (
//     <Router>
//       <Routes>
//         <Route path="/" element={<NotificationsSignInPageError />} />
//         <Route path="/user" element={<UserPage />} />
//         <Route path="/charger" element={<ChargerPage />} />
//       </Routes>
//     </Router>
//   );
// }

// export default App;
