type RoleAttributes =
  | {
      role: 'admin';
      permission: 'all';
    }
  | {
      role: 'user';
    }
  | {
      role: 'manager';
    };

// Union Type: Extract
type AdminRole = Extract<RoleAttributes, { role: 'admin' }>;
const adminRole: AdminRole = {
  role: 'admin',
  permission: 'all',
};
console.log(adminRole);
