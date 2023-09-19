import '../common/index.mjs';
import { stat } from 'fs/promises';

// Should not reject.
stat(new URL(import.meta.url));
