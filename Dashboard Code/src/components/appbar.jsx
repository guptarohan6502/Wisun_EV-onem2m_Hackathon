import * as React from 'react';
import { styled, alpha } from '@mui/material/styles';
import AppBar from '@mui/material/AppBar';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import IconButton from '@mui/material/IconButton';
import Typography from '@mui/material/Typography';
import MenuItem from '@mui/material/MenuItem';
import AccountCircle from '@mui/icons-material/AccountCircle';
import { useNavigate, useLocation } from 'react-router-dom';
import iiithlogo from './iiithlogo.png';
import scrclogo from './scrclogo.png';

const Search = styled('div')(({ theme }) => ({
  position: 'relative',
  borderRadius: theme.shape.borderRadius,
  backgroundColor: alpha(theme.palette.common.white, 0.15),
  '&:hover': {
    backgroundColor: alpha(theme.palette.common.white, 0.25),
  },
  marginRight: theme.spacing(2),
  marginLeft: 0,
  width: '100%',
  [theme.breakpoints.up('sm')]: {
    marginLeft: theme.spacing(3),
    width: 'auto',
  },
}));

export default function PrimarySearchAppBar() {
  const navigate = useNavigate();
  const location = useLocation();
  const [isHovered, setIsHovered] = React.useState(false);

  const handleMouseEnter = () => {
    setIsHovered(true);
  };

  const handleMouseLeave = () => {
    setIsHovered(false);
  };

  const handleLogout = () => {
    navigate('/'); // Redirect to home page
  };

  // Determine the title based on the current path
  const getTitle = () => {
    if (location.pathname === '/charger') {
      return 'Admin Dashboard';
    } else if (location.pathname === '/user') {
      return 'User Dashboard';
    }
    return 'Dashboard';
  };

  return (
    <Box sx={{ flexGrow: 1 }}>
      <AppBar
        position="static"
        sx={{
          backgroundColor: '#05445E',
          height: '8.5vh',
          width: '101vw',
          marginTop: '-1vw',
          marginLeft: '-2vw',
        }}
      >
        <Toolbar>
          <Box sx={{ display: 'flex', alignItems: 'center' }}>
            <Box sx={{ display: 'flex', gap: 2 }}>
              <img src={iiithlogo} alt="IIITH Logo" width={140} height={70} />
              <img src={scrclogo} alt="SCRC Logo" width={140} height={85} />
            </Box>
          </Box>
          <Box sx={{ flexGrow: 1, display: 'flex', justifyContent: 'center' }}>
            <Typography variant="h4" noWrap component="div">
              {getTitle()}
            </Typography>
          </Box>
          <Box
            sx={{ position: 'relative', display: 'flex', alignItems: 'center' }}
            onMouseEnter={handleMouseEnter}
            onMouseLeave={handleMouseLeave}
          >
            <IconButton size="large" edge="end" aria-label="account of current user" color="inherit">
              <AccountCircle />
            </IconButton>
            {isHovered && (
              <Box
                sx={{
                  position: 'absolute',
                  top: '100%',
                  right: 0,
                  backgroundColor: 'black',
                  boxShadow: 1,
                  borderRadius: 1,
                  zIndex: 1000,
                }}
              >
                <MenuItem>User ID: exampleUser</MenuItem>
                <MenuItem onClick={handleLogout}>Logout</MenuItem>
              </Box>
            )}
          </Box>
        </Toolbar>
      </AppBar>
    </Box>
  );
}
