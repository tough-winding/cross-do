import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: { port: 5173, strictPort: true, proxy: { '/api': { target: process.env.CROSSDO_API_PROXY || 'http://127.0.0.1:18080', changeOrigin: true } } },
  build: { target: 'es2022' },
});
